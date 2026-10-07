#include "ModelLoader.h"
#include "Log.h"
#include "cgltf.h"
#include "stb_image.h"
#include "ufbx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>

namespace mehr
{
const char* ChannelName( int channel )
{
	static const char* names[ CH_COUNT ] = { "BaseColor", "Normal", "Roughness", "Metallic", "Height", "Emissive", "AO" };
	return channel >= 0 && channel < CH_COUNT ? names[ channel ] : "?";
}

uint32_t PackSnorm1010102( float x, float y, float z, float w )
{
	auto q10 = []( float v ) -> uint32_t {
		v         = std::max( -1.0f, std::min( 1.0f, v ) );
		int32_t i = (int32_t)std::lround( v * 511.0f );
		return uint32_t( i ) & 0x3FFu;
	};
	uint32_t wq = uint32_t( w < 0.0f ? -1 : 1 ) & 0x3u;
	return q10( x ) | ( q10( y ) << 10 ) | ( q10( z ) << 20 ) | ( wq << 30 );
}

namespace
{
using Clock = std::chrono::steady_clock;
double MsSince( Clock::time_point t0 )
{
	return std::chrono::duration< double, std::milli >( Clock::now() - t0 ).count();
}

// ---------------------------------------------------------------- path helpers
std::string Lower( std::string s )
{
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
	return s;
}
std::string DirName( const std::string& p )
{
	size_t s = p.find_last_of( '/' );
	return s == std::string::npos ? std::string( "." ) : p.substr( 0, s );
}
std::string BaseName( const std::string& p )
{
	size_t s = p.find_last_of( "/\\" );
	return s == std::string::npos ? p : p.substr( s + 1 );
}
std::string Extension( const std::string& p )
{
	std::string b = BaseName( p );
	size_t d      = b.find_last_of( '.' );
	return d == std::string::npos ? std::string() : Lower( b.substr( d + 1 ) );
}
std::string StripExtension( const std::string& name )
{
	size_t d = name.find_last_of( '.' );
	return d == std::string::npos ? name : name.substr( 0, d );
}
bool IsFile( const std::string& p )
{
	struct stat st;
	return !p.empty() && stat( p.c_str(), &st ) == 0 && S_ISREG( st.st_mode );
}
std::string Slashes( std::string s )
{
	std::replace( s.begin(), s.end(), '\\', '/' );
	return s;
}

// Lower-case filename -> full path, for the model folder and a textures/ subfolder (any case).
struct FolderIndex
{
	std::map< std::string, std::string > files;
	void Add( const std::string& dir, int depth )
	{
		DIR* d = opendir( dir.c_str() );
		if( !d )
			return;
		while( dirent* e = readdir( d ) )
		{
			std::string name = e->d_name;
			if( name == "." || name == ".." )
				continue;
			std::string full = dir + "/" + name;
			if( e->d_type == DT_DIR )
			{
				std::string l = Lower( name );
				if( depth == 0 && ( l == "textures" || l == "texture" || l == "tex" || l == "maps" ) )
					Add( full, 1 );
			}
			else
			{
				files.emplace( Lower( name ), full ); // model folder wins over textures/
			}
		}
		closedir( d );
	}
	std::string Find( const std::string& lowerName ) const
	{
		auto it = files.find( lowerName );
		return it == files.end() ? std::string() : it->second;
	}
};

// ---------------------------------------------------------------- image requests
// A texture source: embedded bytes or a file. Decoded once, shared by every channel that uses it.
struct ImageSource
{
	std::string key; // path, or "embedded:<name>#<n>"
	std::string path;
	std::vector< uint8_t > bytes;
	int components = 1;
	bool srgb      = false;
	bool fromSpecGloss = false;
	float glossFactor  = 1.0f; // KHR_materials_pbrSpecularGlossiness map -> R = roughness (1 - A), G = metallic (from specular)
	std::shared_ptr< Image > image = std::make_shared< Image >();
};

struct ImageTable
{
	std::vector< std::unique_ptr< ImageSource > > sources;
	std::unordered_map< std::string, ImageSource* > byKey;
	// components: 1 (scalar data) or 4 (anything with colour); 3-channel uploads cost a driver conversion.
	ImageSource* Get( const std::string& key, int components, bool srgb = false )
	{
		components = components > 1 ? 4 : 1;
		auto it    = byKey.find( key );
		if( it != byKey.end() )
		{
			it->second->components = std::max( it->second->components, components );
			it->second->srgb       = it->second->srgb || srgb;
			return it->second;
		}
		sources.push_back( std::make_unique< ImageSource >() );
		ImageSource* s = sources.back().get();
		s->key         = key;
		s->components  = components;
		s->srgb        = srgb;
		byKey[ key ]   = s;
		return s;
	}
};

int ComponentsFor( int channel )
{
	return ( channel == CH_BASECOLOR || channel == CH_NORMAL || channel == CH_EMISSIVE ) ? 4 : 1;
}
bool IsColorChannel( int channel )
{
	return channel == CH_BASECOLOR || channel == CH_EMISSIVE;
}

// Material description gathered from the file before texture resolution.
struct MaterialSlot
{
	std::string name;
	ImageSource* fileTex[ CH_COUNT ] = {};
	int fileComponent[ CH_COUNT ]    = { -1, -1, -1, -1, -1, -1, -1 };
	Part part; // factors
	bool factorsMultiplyTextures = false; // glTF: yes; FBX/OBJ: texture replaces the factor
};

// ---------------------------------------------------------------- sidecars
struct Alias
{
	int channel;
	const char* names[ 6 ];
	bool dx;
};
const Alias kAliases[] = {
	{ CH_BASECOLOR, { "basecolor", "base_color", "albedo", "diffuse", "color", nullptr }, false },
	{ CH_NORMAL, { "normal", "normalgl", "normal_opengl", "nrm", nullptr }, false },
	{ CH_NORMAL, { "normaldx", "normal_directx", nullptr }, true },
	{ CH_ROUGHNESS, { "roughness", "rough", nullptr }, false },
	{ CH_METALLIC, { "metallic", "metalness", "metal", nullptr }, false },
	{ CH_HEIGHT, { "height", "displacement", "disp", nullptr }, false },
	{ CH_EMISSIVE, { "emissive", "emission", nullptr }, false },
	{ CH_AO, { "ao", "ambientocclusion", "occlusion", nullptr }, false },
};
const char* kImageExts[] = { "png", "jpg", "jpeg", "tga", "bmp", "hdr", "exr" };

std::vector< std::string > MaterialNameVariants( const std::string& name )
{
	std::vector< std::string > v;
	auto push = [ & ]( std::string s ) {
		s = Lower( s );
		if( !s.empty() && std::find( v.begin(), v.end(), s ) == v.end() )
			v.push_back( s );
	};
	push( name );
	std::string spaced = name;
	std::replace( spaced.begin(), spaced.end(), ' ', '_' );
	push( spaced );
	// Blender duplicates: "<name>.001"
	size_t dot = name.find_last_of( '.' );
	if( dot != std::string::npos && dot + 1 < name.size() && std::all_of( name.begin() + dot + 1, name.end(), ::isdigit ) )
		push( name.substr( 0, dot ) );
	return v;
}

// Returns the path of a sidecar image for (material, alias), exact "<mat>_<alias>.<ext>" first,
// then "<anything>_<mat>_<alias>.<ext>" (Substance "<mesh>_<set>_<channel>" exports).
std::string FindSidecar( const FolderIndex& folder, const std::vector< std::string >& matNames, const char* alias, bool& unsupported )
{
	unsupported = false;
	for( const std::string& m : matNames )
		for( const char* ext : kImageExts )
		{
			std::string p = folder.Find( m + "_" + alias + "." + ext );
			if( !p.empty() )
			{
				unsupported = std::string( ext ) == "exr";
				if( !unsupported )
					return p;
			}
		}
	for( const std::string& m : matNames )
		for( const auto& kv : folder.files )
		{
			const std::string& f = kv.first;
			std::string st       = StripExtension( f );
			std::string suffix   = "_" + m + "_" + alias;
			if( st.size() > suffix.size() && st.compare( st.size() - suffix.size(), suffix.size(), suffix ) == 0 )
			{
				std::string ext = Extension( f );
				if( std::find( std::begin( kImageExts ), std::end( kImageExts ), ext ) == std::end( kImageExts ) || ext == "exr" )
					continue;
				return kv.second;
			}
		}
	return {};
}

// ---------------------------------------------------------------- geometry builder
struct Corner
{
	float p[ 3 ];
	float n[ 3 ];
	float uv[ 2 ];
	float t[ 4 ];
};

struct MeshNode
{
	std::string name, parent; // the file's node name and its parent's (display names for parts split by node)
};

struct MeshBuilder
{
	std::vector< Corner > corners; // 3 per triangle
	std::vector< uint32_t > triSlot; // material slot per triangle (the draw group after GroupParts)
	std::vector< uint32_t > triNode; // index into nodes
	std::vector< MeshNode > nodes;
	uint32_t node        = 0;
	bool allHaveTangents = true;

	void BeginNode( const std::string& name, const std::string& parent )
	{
		node = (uint32_t)nodes.size();
		nodes.push_back( { name, parent } );
	}
	void AddTriangle( const Corner& a, const Corner& b, const Corner& c, uint32_t slot )
	{
		corners.push_back( a );
		corners.push_back( b );
		corners.push_back( c );
		triSlot.push_back( slot );
		triNode.push_back( node );
	}
};

inline uint64_t HashBytes( const void* data, size_t n )
{
	const uint8_t* p = (const uint8_t*)data;
	uint64_t h       = 1469598103934665603ull;
	for( size_t i = 0; i < n; ++i )
		h = ( h ^ p[ i ] ) * 1099511628211ull;
	return h ^ ( h >> 29 );
}

inline void Normalize3( float* v )
{
	float l = std::sqrt( v[ 0 ] * v[ 0 ] + v[ 1 ] * v[ 1 ] + v[ 2 ] * v[ 2 ] );
	if( l > 1e-20f )
	{
		v[ 0 ] /= l;
		v[ 1 ] /= l;
		v[ 2 ] /= l;
	}
	else
	{
		v[ 0 ] = 0;
		v[ 1 ] = 1;
		v[ 2 ] = 0;
	}
}

// Dedupe corners, generate tangents if the source had none, sort triangles by draw group (mb.triSlot = the
// index into `parts`), and give every part its index range and bbox.
void FinalizeGeometry( MeshBuilder& mb, std::vector< Part >& parts, ModelData& out )
{
	const size_t numCorners = mb.corners.size();
	const size_t numTris    = numCorners / 3;
	const bool keepTangents = mb.allHaveTangents;
	const size_t keyBytes   = keepTangents ? sizeof( Corner ) : offsetof( Corner, t );

	for( Corner& c : mb.corners )
		for( float& f : c.p )
			if( f == 0.0f )
				f = 0.0f; // fold -0

	// open-addressing dedupe
	size_t cap = 1;
	while( cap < numCorners * 2 )
		cap <<= 1;
	std::vector< uint32_t > table( cap, UINT32_MAX );
	std::vector< Corner > unique;
	unique.reserve( numCorners / 2 );
	std::vector< uint32_t > cornerToVert( numCorners );
	for( size_t i = 0; i < numCorners; ++i )
	{
		const Corner& c = mb.corners[ i ];
		size_t h        = HashBytes( &c, keyBytes ) & ( cap - 1 );
		for( ;; )
		{
			uint32_t slot = table[ h ];
			if( slot == UINT32_MAX )
			{
				table[ h ] = (uint32_t)unique.size();
				unique.push_back( c );
				cornerToVert[ i ] = table[ h ];
				break;
			}
			if( std::memcmp( &unique[ slot ], &c, keyBytes ) == 0 )
			{
				cornerToVert[ i ] = slot;
				break;
			}
			h = ( h + 1 ) & ( cap - 1 );
		}
	}
	std::vector< uint32_t >().swap( table );

	if( !keepTangents )
	{
		// Lengyel per-vertex tangents. Convention: T = dP/ds, green/up bitangent = -dP/dt
		// (t = 0 is the image top), w = handedness so that B = cross(N, T) * w.
		std::vector< float > tan( unique.size() * 3, 0.0f ), bit( unique.size() * 3, 0.0f );
		for( size_t t = 0; t < numTris; ++t )
		{
			uint32_t i0 = cornerToVert[ t * 3 ], i1 = cornerToVert[ t * 3 + 1 ], i2 = cornerToVert[ t * 3 + 2 ];
			const Corner &a = unique[ i0 ], &b = unique[ i1 ], &c = unique[ i2 ];
			float e1[ 3 ] = { b.p[ 0 ] - a.p[ 0 ], b.p[ 1 ] - a.p[ 1 ], b.p[ 2 ] - a.p[ 2 ] };
			float e2[ 3 ] = { c.p[ 0 ] - a.p[ 0 ], c.p[ 1 ] - a.p[ 1 ], c.p[ 2 ] - a.p[ 2 ] };
			float du1 = b.uv[ 0 ] - a.uv[ 0 ], dv1 = b.uv[ 1 ] - a.uv[ 1 ];
			float du2 = c.uv[ 0 ] - a.uv[ 0 ], dv2 = c.uv[ 1 ] - a.uv[ 1 ];
			float r = du1 * dv2 - du2 * dv1;
			if( std::fabs( r ) < 1e-20f )
				continue;
			float inv = 1.0f / r;
			float sd[ 3 ], td[ 3 ];
			for( int k = 0; k < 3; ++k )
			{
				sd[ k ] = ( e1[ k ] * dv2 - e2[ k ] * dv1 ) * inv;
				td[ k ] = ( e2[ k ] * du1 - e1[ k ] * du2 ) * inv;
			}
			for( uint32_t vi : { i0, i1, i2 } )
				for( int k = 0; k < 3; ++k )
				{
					tan[ vi * 3 + k ] += sd[ k ];
					bit[ vi * 3 + k ] += td[ k ];
				}
		}
		for( size_t v = 0; v < unique.size(); ++v )
		{
			Corner& c      = unique[ v ];
			const float* n = c.n;
			float* t       = &tan[ v * 3 ];
			float d        = n[ 0 ] * t[ 0 ] + n[ 1 ] * t[ 1 ] + n[ 2 ] * t[ 2 ];
			float T[ 3 ]   = { t[ 0 ] - n[ 0 ] * d, t[ 1 ] - n[ 1 ] * d, t[ 2 ] - n[ 2 ] * d };
			float len      = std::sqrt( T[ 0 ] * T[ 0 ] + T[ 1 ] * T[ 1 ] + T[ 2 ] * T[ 2 ] );
			if( len < 1e-12f )
			{
				// no usable UV gradient: any vector perpendicular to N
				float a[ 3 ] = { std::fabs( n[ 0 ] ) < 0.9f ? 1.0f : 0.0f, std::fabs( n[ 0 ] ) < 0.9f ? 0.0f : 1.0f, 0.0f };
				T[ 0 ]       = a[ 1 ] * n[ 2 ] - a[ 2 ] * n[ 1 ];
				T[ 1 ]       = a[ 2 ] * n[ 0 ] - a[ 0 ] * n[ 2 ];
				T[ 2 ]       = a[ 0 ] * n[ 1 ] - a[ 1 ] * n[ 0 ];
			}
			Normalize3( T );
			float cr[ 3 ] = { n[ 1 ] * T[ 2 ] - n[ 2 ] * T[ 1 ], n[ 2 ] * T[ 0 ] - n[ 0 ] * T[ 2 ], n[ 0 ] * T[ 1 ] - n[ 1 ] * T[ 0 ] };
			const float* b = &bit[ v * 3 ];
			float up       = -( cr[ 0 ] * b[ 0 ] + cr[ 1 ] * b[ 1 ] + cr[ 2 ] * b[ 2 ] );
			c.t[ 0 ]       = T[ 0 ];
			c.t[ 1 ]       = T[ 1 ];
			c.t[ 2 ]       = T[ 2 ];
			c.t[ 3 ]       = up < 0.0f ? -1.0f : 1.0f;
		}
	}

	// pack vertices + bbox
	out.vertices.resize( unique.size() );
	float mn[ 3 ] = { 1e30f, 1e30f, 1e30f }, mx[ 3 ] = { -1e30f, -1e30f, -1e30f };
	for( size_t v = 0; v < unique.size(); ++v )
	{
		const Corner& c = unique[ v ];
		Vertex& o       = out.vertices[ v ];
		o.px = c.p[ 0 ], o.py = c.p[ 1 ], o.pz = c.p[ 2 ];
		o.u = c.uv[ 0 ], o.v = c.uv[ 1 ];
		o.normal  = PackSnorm1010102( c.n[ 0 ], c.n[ 1 ], c.n[ 2 ], 1.0f );
		o.tangent = PackSnorm1010102( c.t[ 0 ], c.t[ 1 ], c.t[ 2 ], c.t[ 3 ] );
		for( int k = 0; k < 3; ++k )
		{
			mn[ k ] = std::min( mn[ k ], c.p[ k ] );
			mx[ k ] = std::max( mx[ k ], c.p[ k ] );
		}
	}
	if( unique.empty() )
		mn[ 0 ] = mn[ 1 ] = mn[ 2 ] = mx[ 0 ] = mx[ 1 ] = mx[ 2 ] = 0.0f;
	float maxDim = 0.0f;
	for( int k = 0; k < 3; ++k )
	{
		out.bboxMin[ k ]   = mn[ k ];
		out.bboxMax[ k ]   = mx[ k ];
		out.fitCenter[ k ] = 0.5f * ( mn[ k ] + mx[ k ] );
		maxDim             = std::max( maxDim, mx[ k ] - mn[ k ] );
	}
	out.fitScale = maxDim > 1e-12f ? 2.0f / maxDim : 1.0f;

	// counting sort of triangles by part -> contiguous index ranges per part
	std::vector< uint32_t > counts( parts.size(), 0 );
	for( uint32_t s : mb.triSlot )
		counts[ s ]++;
	std::vector< uint32_t > start( parts.size(), 0 );
	for( size_t s = 1; s < parts.size(); ++s )
		start[ s ] = start[ s - 1 ] + counts[ s - 1 ];
	out.indices.resize( numCorners );
	std::vector< uint32_t > cursor = start;
	for( size_t t = 0; t < numTris; ++t )
	{
		uint32_t dst = cursor[ mb.triSlot[ t ] ]++ * 3;
		for( int k = 0; k < 3; ++k )
			out.indices[ dst + k ] = cornerToVert[ t * 3 + k ];
	}
	for( size_t s = 0; s < parts.size(); ++s )
	{
		Part& p      = parts[ s ];
		p.firstIndex = start[ s ] * 3;
		p.indexCount = counts[ s ] * 3;
		for( int k = 0; k < 3; ++k )
			p.boxMin[ k ] = 1e30f, p.boxMax[ k ] = -1e30f;
		for( uint32_t i = p.firstIndex; i < p.firstIndex + p.indexCount; ++i )
		{
			const Vertex& v = out.vertices[ out.indices[ i ] ];
			const float q[ 3 ] = { v.px, v.py, v.pz };
			for( int k = 0; k < 3; ++k )
				p.boxMin[ k ] = std::min( p.boxMin[ k ], q[ k ] ), p.boxMax[ k ] = std::max( p.boxMax[ k ], q[ k ] );
		}
	}
	out.triangleCount = numTris;
}

// ---------------------------------------------------------------- ufbx (fbx / obj)
std::string UfbxStr( const ufbx_string& s )
{
	return std::string( s.data, s.length );
}

ImageSource* UfbxTexture( ufbx_texture* tex, const std::string& modelDir, const FolderIndex& folder, ImageTable& images, int components, bool srgb, std::string& how )
{
	if( !tex )
		return nullptr;
	if( tex->type != UFBX_TEXTURE_FILE && tex->file_textures.count > 0 )
		tex = tex->file_textures.data[ 0 ];

	const ufbx_blob* blob = nullptr;
	if( tex->content.size > 0 )
		blob = &tex->content;
	else if( tex->video && tex->video->content.size > 0 )
		blob = &tex->video->content;
	std::string rel = Slashes( UfbxStr( tex->relative_filename ) );
	if( blob )
	{
		std::string key = "embedded:" + ( rel.empty() ? UfbxStr( tex->name ) : rel );
		ImageSource* s  = images.Get( key, components, srgb );
		if( s->bytes.empty() )
			s->bytes.assign( (const uint8_t*)blob->data, (const uint8_t*)blob->data + blob->size );
		how = "embedded '" + rel + "'";
		return s;
	}
	std::vector< std::string > candidates = {
		Slashes( UfbxStr( tex->filename ) ),
		Slashes( UfbxStr( tex->absolute_filename ) ),
		rel.empty() ? std::string() : modelDir + "/" + rel,
	};
	std::string base = BaseName( rel.empty() ? Slashes( UfbxStr( tex->absolute_filename ) ) : rel );
	for( const std::string& c : candidates )
		if( IsFile( c ) )
		{
			how = "file path " + c;
			ImageSource* s = images.Get( c, components, srgb );
			s->path        = c;
			return s;
		}
	if( !base.empty() )
	{
		std::string p = folder.Find( Lower( base ) );
		if( !p.empty() )
		{
			how = "file name '" + base + "' found at " + p;
			ImageSource* s = images.Get( p, components, srgb );
			s->path        = p;
			return s;
		}
		Log( "  texture '%s' referenced by the file was not found", rel.c_str() );
	}
	return nullptr;
}

bool LoadUfbx( const std::string& path, const std::atomic< bool >& cancel, MeshBuilder& mb, std::vector< MaterialSlot >& slots, ImageTable& images, const FolderIndex& folder, std::string& error )
{
	ufbx_load_opts opts                = {};
	opts.target_axes                   = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters            = 1.0f;
	opts.space_conversion              = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	opts.generate_missing_normals      = true;
	opts.ignore_missing_external_files = true;
	if( Extension( path ) == "obj" )
	{
		// .mtl is an external file; allow it (and try "<model>.mtl" if the mtllib name is wrong)
		opts.load_external_files        = true;
		opts.obj_search_mtl_by_filename = true;
	}
	opts.progress_cb.fn                = []( void* user, const ufbx_progress* ) -> ufbx_progress_result {
		return ( (const std::atomic< bool >*)user )->load() ? UFBX_PROGRESS_CANCEL : UFBX_PROGRESS_CONTINUE;
	};
	opts.progress_cb.user = (void*)&cancel;

	ufbx_error err;
	ufbx_scene* scene = ufbx_load_file( path.c_str(), &opts, &err );
	if( !scene )
	{
		char buf[ 512 ];
		ufbx_format_error( buf, sizeof( buf ), &err );
		error = std::string( "ufbx: " ) + buf;
		return false;
	}
	std::string modelDir = DirName( path );

	std::unordered_map< const ufbx_material*, uint32_t > slotOf;
	auto slotFor = [ & ]( const ufbx_material* m ) -> uint32_t {
		auto it = slotOf.find( m );
		if( it != slotOf.end() )
			return it->second;
		uint32_t idx = (uint32_t)slots.size();
		slotOf[ m ]  = idx;
		slots.emplace_back();
		MaterialSlot& s = slots.back();
		s.name          = m ? UfbxStr( m->name ) : std::string( "default" );
		if( s.name.empty() )
			s.name = "material" + std::to_string( idx + 1 );
		if( m )
		{
			const ufbx_material_pbr_maps& pbr = m->pbr;
			const ufbx_material_fbx_maps& fbx = m->fbx;
			if( pbr.base_color.has_value )
			{
				float f = pbr.base_factor.has_value ? (float)pbr.base_factor.value_real : 1.0f;
				for( int k = 0; k < 3; ++k )
					s.part.baseColor[ k ] = (float)pbr.base_color.value_vec4.v[ k ] * f;
			}
			if( pbr.roughness.has_value )
				s.part.roughness = std::min( 1.0f, std::max( 0.02f, (float)pbr.roughness.value_real ) );
			if( pbr.metalness.has_value )
				s.part.metallic = std::min( 1.0f, std::max( 0.0f, (float)pbr.metalness.value_real ) );
			if( pbr.emission_color.has_value )
			{
				float f = pbr.emission_factor.has_value ? (float)pbr.emission_factor.value_real : 1.0f;
				for( int k = 0; k < 3; ++k )
					s.part.emissive[ k ] = (float)pbr.emission_color.value_vec3.v[ k ] * f;
			}
			ufbx_texture* texs[ CH_COUNT ] = {
				pbr.base_color.texture ? pbr.base_color.texture : fbx.diffuse_color.texture,
				pbr.normal_map.texture ? pbr.normal_map.texture : fbx.normal_map.texture,
				pbr.roughness.texture,
				pbr.metalness.texture,
				pbr.displacement_map.texture ? pbr.displacement_map.texture : fbx.displacement.texture,
				pbr.emission_color.texture ? pbr.emission_color.texture : fbx.emission_color.texture,
				pbr.ambient_occlusion.texture,
			};
			for( int ch = 0; ch < CH_COUNT; ++ch )
			{
				std::string how;
				s.fileTex[ ch ] = UfbxTexture( texs[ ch ], modelDir, folder, images, ComponentsFor( ch ), IsColorChannel( ch ), how );
				if( s.fileTex[ ch ] )
					Log( "  [%s] %s <- %s", s.name.c_str(), ChannelName( ch ), how.c_str() );
			}
		}
		return idx;
	};

	std::vector< uint32_t > triIdx;
	for( size_t ni = 0; ni < scene->nodes.count; ++ni )
	{
		ufbx_node* node = scene->nodes.data[ ni ];
		ufbx_mesh* mesh = node->mesh;
		if( !mesh || node->visible == false )
			continue;
		if( cancel.load() )
			break;
		mb.BeginNode( UfbxStr( node->name ), node->parent ? UfbxStr( node->parent->name ) : std::string() );
		ufbx_matrix posM = node->geometry_to_world;
		ufbx_matrix nrmM = ufbx_matrix_for_normals( &posM );
		triIdx.resize( mesh->max_face_triangles * 3 );
		const bool hasUV  = mesh->vertex_uv.exists;
		const bool hasTan = mesh->vertex_tangent.exists && mesh->vertex_bitangent.exists;
		if( !hasTan )
			mb.allHaveTangents = false;
		for( size_t fi = 0; fi < mesh->faces.count; ++fi )
		{
			ufbx_face face = mesh->faces.data[ fi ];
			if( face.num_indices < 3 )
				continue;
			const ufbx_material* mat = nullptr;
			if( mesh->face_material.count > 0 )
			{
				uint32_t mi = mesh->face_material.data[ fi ];
				if( mi < node->materials.count )
					mat = node->materials.data[ mi ];
				else if( mi < mesh->materials.count )
					mat = mesh->materials.data[ mi ];
			}
			uint32_t slot = slotFor( mat );
			uint32_t nt   = ufbx_triangulate_face( triIdx.data(), triIdx.size(), mesh, face );
			for( uint32_t t = 0; t < nt; ++t )
			{
				Corner c[ 3 ];
				for( int k = 0; k < 3; ++k )
				{
					uint32_t ix = triIdx[ t * 3 + k ];
					ufbx_vec3 p = ufbx_transform_position( &posM, ufbx_get_vertex_vec3( &mesh->vertex_position, ix ) );
					ufbx_vec3 n = ufbx_transform_direction( &nrmM, ufbx_get_vertex_vec3( &mesh->vertex_normal, ix ) );
					c[ k ].p[ 0 ] = (float)p.x, c[ k ].p[ 1 ] = (float)p.y, c[ k ].p[ 2 ] = (float)p.z;
					c[ k ].n[ 0 ] = (float)n.x, c[ k ].n[ 1 ] = (float)n.y, c[ k ].n[ 2 ] = (float)n.z;
					Normalize3( c[ k ].n );
					if( hasUV )
					{
						ufbx_vec2 uv  = ufbx_get_vertex_vec2( &mesh->vertex_uv, ix );
						c[ k ].uv[ 0 ] = (float)uv.x;
						c[ k ].uv[ 1 ] = 1.0f - (float)uv.y; // FBX/OBJ: v up -> t = 0 at the image top
					}
					else
						c[ k ].uv[ 0 ] = c[ k ].uv[ 1 ] = 0.0f;
					c[ k ].t[ 0 ] = c[ k ].t[ 1 ] = c[ k ].t[ 2 ] = 0.0f;
					c[ k ].t[ 3 ] = 1.0f;
					if( hasTan )
					{
						ufbx_vec3 tg = ufbx_transform_direction( &posM, ufbx_get_vertex_vec3( &mesh->vertex_tangent, ix ) );
						ufbx_vec3 bt = ufbx_transform_direction( &posM, ufbx_get_vertex_vec3( &mesh->vertex_bitangent, ix ) );
						float T[ 3 ] = { (float)tg.x, (float)tg.y, (float)tg.z };
						Normalize3( T );
						const float* N = c[ k ].n;
						float cr[ 3 ]  = { N[ 1 ] * T[ 2 ] - N[ 2 ] * T[ 1 ], N[ 2 ] * T[ 0 ] - N[ 0 ] * T[ 2 ], N[ 0 ] * T[ 1 ] - N[ 1 ] * T[ 0 ] };
						// FBX bitangent follows +v (up), which is the normal-map green direction.
						float w = cr[ 0 ] * bt.x + cr[ 1 ] * bt.y + cr[ 2 ] * bt.z;
						c[ k ].t[ 0 ] = T[ 0 ], c[ k ].t[ 1 ] = T[ 1 ], c[ k ].t[ 2 ] = T[ 2 ];
						c[ k ].t[ 3 ] = w < 0 ? -1.0f : 1.0f;
					}
				}
				mb.AddTriangle( c[ 0 ], c[ 1 ], c[ 2 ], slot );
			}
		}
	}
	ufbx_free_scene( scene );
	if( cancel.load() )
	{
		error = "cancelled";
		return false;
	}
	return true;
}

// ---------------------------------------------------------------- cgltf (gltf / glb)
ImageSource* GltfImage( const cgltf_texture_view& view, const cgltf_data* data, const std::string& modelDir, ImageTable& images, int components, bool srgb, std::string& how )
{
	if( !view.texture )
		return nullptr;
	const cgltf_image* img = view.texture->image;
	if( !img )
	{
		Log( "  texture uses an image format this build cannot decode (basisu/webp/dds), skipped" );
		return nullptr;
	}
	if( view.texcoord != 0 )
		Log( "  texture uses TEXCOORD_%d; only TEXCOORD_0 is loaded", view.texcoord );
	size_t imgIndex = size_t( img - data->images );
	if( img->buffer_view )
	{
		std::string key = "embedded:image#" + std::to_string( imgIndex );
		ImageSource* s  = images.Get( key, components, srgb );
		if( s->bytes.empty() )
		{
			const uint8_t* p = cgltf_buffer_view_data( img->buffer_view );
			if( p )
				s->bytes.assign( p, p + img->buffer_view->size );
		}
		how = "embedded image #" + std::to_string( imgIndex );
		return s;
	}
	if( img->uri )
	{
		std::string uri = img->uri;
		if( uri.compare( 0, 5, "data:" ) == 0 )
		{
			size_t comma = uri.find( ',' );
			if( comma == std::string::npos || uri.find( ";base64" ) == std::string::npos )
				return nullptr;
			std::string key = "embedded:image#" + std::to_string( imgIndex );
			ImageSource* s  = images.Get( key, components, srgb );
			if( s->bytes.empty() )
			{
				std::string b64 = uri.substr( comma + 1 );
				size_t size     = b64.size() * 3 / 4;
				while( !b64.empty() && b64.back() == '=' )
				{
					b64.pop_back();
					size--;
				}
				cgltf_options o = {};
				void* out       = nullptr;
				if( cgltf_load_buffer_base64( &o, size, b64.c_str(), &out ) == cgltf_result_success )
				{
					s->bytes.assign( (uint8_t*)out, (uint8_t*)out + size );
					free( out );
				}
			}
			how = "data URI image #" + std::to_string( imgIndex );
			return s;
		}
		std::vector< char > dec( uri.begin(), uri.end() );
		dec.push_back( 0 );
		cgltf_decode_uri( dec.data() );
		std::string p = modelDir + "/" + Slashes( dec.data() );
		if( IsFile( p ) )
		{
			how = "file " + p;
			ImageSource* s = images.Get( p, components, srgb );
			s->path        = p;
			return s;
		}
		Log( "  image '%s' referenced by the file was not found", uri.c_str() );
	}
	return nullptr;
}

void GltfMatrixMul( const float* m, const float* v, float w, float* out )
{
	for( int r = 0; r < 3; ++r )
		out[ r ] = m[ 0 * 4 + r ] * v[ 0 ] + m[ 1 * 4 + r ] * v[ 1 ] + m[ 2 * 4 + r ] * v[ 2 ] + m[ 3 * 4 + r ] * w;
}

bool Invert3x3Transpose( const float* m4, float* out9 )
{
	// normal matrix = inverse-transpose of the upper 3x3 (column-major input)
	float a = m4[ 0 ], b = m4[ 4 ], c = m4[ 8 ];
	float d = m4[ 1 ], e = m4[ 5 ], f = m4[ 9 ];
	float g = m4[ 2 ], h = m4[ 6 ], i = m4[ 10 ];
	float A = e * i - f * h, B = -( d * i - f * g ), C = d * h - e * g;
	float det = a * A + b * B + c * C;
	if( std::fabs( det ) < 1e-30f )
		return false;
	float id = 1.0f / det;
	// cofactor matrix / det = inverse-transpose; out as row-major 3x3 (row r = output component r)
	out9[ 0 ] = A * id, out9[ 1 ] = B * id, out9[ 2 ] = C * id;
	out9[ 3 ] = -( b * i - c * h ) * id, out9[ 4 ] = ( a * i - c * g ) * id, out9[ 5 ] = -( a * h - b * g ) * id;
	out9[ 6 ] = ( b * f - c * e ) * id, out9[ 7 ] = -( a * f - c * d ) * id, out9[ 8 ] = ( a * e - b * d ) * id;
	return true;
}

bool LoadGltf( const std::string& path, const std::atomic< bool >& cancel, MeshBuilder& mb, std::vector< MaterialSlot >& slots, ImageTable& images, std::string& error )
{
	cgltf_options opts = {};
	cgltf_data* data   = nullptr;
	if( cgltf_parse_file( &opts, path.c_str(), &data ) != cgltf_result_success )
	{
		error = "cgltf: could not parse file";
		return false;
	}
	if( cgltf_load_buffers( &opts, data, path.c_str() ) != cgltf_result_success )
	{
		cgltf_free( data );
		error = "cgltf: could not load buffers (missing .bin?)";
		return false;
	}
	std::string modelDir = DirName( path );

	std::unordered_map< const cgltf_material*, uint32_t > slotOf;
	auto slotFor = [ & ]( const cgltf_material* m ) -> uint32_t {
		auto it = slotOf.find( m );
		if( it != slotOf.end() )
			return it->second;
		uint32_t idx = (uint32_t)slots.size();
		slotOf[ m ]  = idx;
		slots.emplace_back();
		MaterialSlot& s           = slots.back();
		s.factorsMultiplyTextures = true;
		s.name                    = m && m->name ? m->name : ( m ? "material" + std::to_string( idx + 1 ) : "default" );
		if( !m )
		{
			s.part.baseColor[ 0 ] = s.part.baseColor[ 1 ] = s.part.baseColor[ 2 ] = 1.0f;
			s.part.roughness                                              = 1.0f;
			s.part.metallic                                               = 1.0f; // glTF defaults
			return idx;
		}
		std::string how;
		if( m->has_pbr_metallic_roughness )
		{
			const cgltf_pbr_metallic_roughness& pbr = m->pbr_metallic_roughness;
			for( int k = 0; k < 4; ++k )
				s.part.baseColor[ k ] = pbr.base_color_factor[ k ];
			s.part.metallic  = pbr.metallic_factor;
			s.part.roughness = pbr.roughness_factor;
			if( ( s.fileTex[ CH_BASECOLOR ] = GltfImage( pbr.base_color_texture, data, modelDir, images, 4, true, how ) ) )
				Log( "  [%s] BaseColor <- %s", s.name.c_str(), how.c_str() );
			if( ImageSource* mr = GltfImage( pbr.metallic_roughness_texture, data, modelDir, images, 4, false, how ) )
			{
				s.fileTex[ CH_ROUGHNESS ] = mr, s.fileComponent[ CH_ROUGHNESS ] = 1;
				s.fileTex[ CH_METALLIC ] = mr, s.fileComponent[ CH_METALLIC ] = 2;
				Log( "  [%s] Roughness(G) + Metallic(B) <- %s", s.name.c_str(), how.c_str() );
			}
		}
		else if( m->has_pbr_specular_glossiness )
		{
			// Spec/gloss -> metal/rough: diffuse is the base colour, roughness = 1 - glossiness, metallic from the
			// specular level (dielectrics sit at ~0.04). Factors multiply the textures as usual.
			const cgltf_pbr_specular_glossiness& sg = m->pbr_specular_glossiness;
			for( int k = 0; k < 4; ++k )
				s.part.baseColor[ k ] = sg.diffuse_factor[ k ];
			float spec       = std::max( sg.specular_factor[ 0 ], std::max( sg.specular_factor[ 1 ], sg.specular_factor[ 2 ] ) );
			s.part.metallic  = std::clamp( ( spec - 0.04f ) / 0.46f, 0.0f, 1.0f );
			s.part.roughness = 1.0f - sg.glossiness_factor;
			if( ( s.fileTex[ CH_BASECOLOR ] = GltfImage( sg.diffuse_texture, data, modelDir, images, 4, true, how ) ) )
				Log( "  [%s] BaseColor <- %s (specular-glossiness diffuse)", s.name.c_str(), how.c_str() );
			if( ImageSource* src = GltfImage( sg.specular_glossiness_texture, data, modelDir, images, 4, false, how ) )
			{
				ImageSource* d = images.Get( src->key + "#specgloss", 4 );
				d->path = src->path, d->bytes = src->bytes, d->fromSpecGloss = true;
				s.fileTex[ CH_ROUGHNESS ] = d, s.fileComponent[ CH_ROUGHNESS ] = 0;
				s.fileTex[ CH_METALLIC ] = d, s.fileComponent[ CH_METALLIC ] = 1;
				// with a map, the factors scale it: roughness = 1 - gloss x A, so the factor is applied in the conversion
				s.part.roughness = 1.0f, s.part.metallic = 1.0f;
				d->glossFactor = sg.glossiness_factor;
				Log( "  [%s] Roughness(1 - A) + Metallic(specular) <- %s (specular-glossiness)", s.name.c_str(), how.c_str() );
			}
		}
		if( ( s.fileTex[ CH_NORMAL ] = GltfImage( m->normal_texture, data, modelDir, images, 4, false, how ) ) )
		{
			s.part.normalScale = m->normal_texture.scale;
			Log( "  [%s] Normal <- %s", s.name.c_str(), how.c_str() );
		}
		if( ( s.fileTex[ CH_AO ] = GltfImage( m->occlusion_texture, data, modelDir, images, 4, false, how ) ) )
		{
			s.fileComponent[ CH_AO ] = 0;
			Log( "  [%s] AO(R) <- %s", s.name.c_str(), how.c_str() );
		}
		if( ( s.fileTex[ CH_EMISSIVE ] = GltfImage( m->emissive_texture, data, modelDir, images, 4, true, how ) ) )
			Log( "  [%s] Emissive <- %s", s.name.c_str(), how.c_str() );
		float es = m->has_emissive_strength ? m->emissive_strength.emissive_strength : 1.0f;
		for( int k = 0; k < 3; ++k )
			s.part.emissive[ k ] = m->emissive_factor[ k ] * es;
		// BLEND has no sorted transparency here: it is drawn as a cutout at 0.5 (petals, decals, hair cards keep their
		// shape; a half-clear lens factor of >= 0.5 stays solid)
		s.part.alphaCutout = m->alpha_mode == cgltf_alpha_mode_mask || m->alpha_mode == cgltf_alpha_mode_blend;
		// W18: see-through materials get the Glass surface on their part: KHR_materials_transmission at its factor,
		// BLEND with a low base alpha (lenses, visors) at 2 x (1 - alpha). The rest of BLEND stays a 0.5 cutout.
		const float alpha = m->has_pbr_metallic_roughness ? m->pbr_metallic_roughness.base_color_factor[ 3 ]
		                  : m->has_pbr_specular_glossiness ? m->pbr_specular_glossiness.diffuse_factor[ 3 ] : 1.0f;
		if( m->has_transmission && m->transmission.transmission_factor > 0.0f )
			s.part.glass = std::min( 1.0f, m->transmission.transmission_factor );
		else if( m->alpha_mode == cgltf_alpha_mode_blend && alpha <= 0.75f )
			s.part.glass = std::min( 1.0f, 2.0f * ( 1.0f - alpha ) );
		if( s.part.glass > 0.0f )
		{
			s.part.alphaCutout = false;
			Log( "  [%s] %s -> Glass surface at %.2f", s.name.c_str(), m->has_transmission ? "KHR_materials_transmission" : "alphaMode BLEND with a low base alpha", s.part.glass );
		}
		else if( m->alpha_mode == cgltf_alpha_mode_blend )
			Log( "  [%s] alphaMode BLEND drawn as a cutout at alpha 0.5", s.name.c_str() );
		return idx;
	};

	std::vector< cgltf_node* > stack;
	const cgltf_scene* scene = data->scene ? data->scene : ( data->scenes_count ? &data->scenes[ 0 ] : nullptr );
	if( scene )
		for( size_t i = 0; i < scene->nodes_count; ++i )
			stack.push_back( scene->nodes[ i ] );
	else
		for( size_t i = 0; i < data->nodes_count; ++i )
			if( !data->nodes[ i ].parent )
				stack.push_back( &data->nodes[ i ] );

	while( !stack.empty() && !cancel.load() )
	{
		cgltf_node* node = stack.back();
		stack.pop_back();
		for( size_t i = 0; i < node->children_count; ++i )
			stack.push_back( node->children[ i ] );
		if( !node->mesh )
			continue;
		mb.BeginNode( node->name ? node->name : ( node->mesh->name ? node->mesh->name : "" ), node->parent && node->parent->name ? node->parent->name : "" );
		float world[ 16 ];
		cgltf_node_transform_world( node, world );
		float nrm[ 9 ];
		if( !Invert3x3Transpose( world, nrm ) )
			continue;
		for( size_t pi = 0; pi < node->mesh->primitives_count; ++pi )
		{
			const cgltf_primitive& prim = node->mesh->primitives[ pi ];
			if( prim.type != cgltf_primitive_type_triangles && prim.type != cgltf_primitive_type_triangle_strip && prim.type != cgltf_primitive_type_triangle_fan )
				continue;
			const cgltf_accessor *pos = nullptr, *nor = nullptr, *uv = nullptr, *tan = nullptr;
			for( size_t a = 0; a < prim.attributes_count; ++a )
			{
				const cgltf_attribute& at = prim.attributes[ a ];
				if( at.type == cgltf_attribute_type_position && at.index == 0 )
					pos = at.data;
				else if( at.type == cgltf_attribute_type_normal && at.index == 0 )
					nor = at.data;
				else if( at.type == cgltf_attribute_type_texcoord && at.index == 0 )
					uv = at.data;
				else if( at.type == cgltf_attribute_type_tangent && at.index == 0 )
					tan = at.data;
			}
			if( !pos )
				continue;
			if( !tan || !nor )
				mb.allHaveTangents = false;
			size_t nv = pos->count;
			std::vector< float > P( nv * 3 ), N( nv * 3, 0.0f ), UV( nv * 2, 0.0f ), T( nv * 4, 0.0f );
			for( size_t v = 0; v < nv; ++v )
			{
				float p[ 3 ];
				cgltf_accessor_read_float( pos, v, p, 3 );
				GltfMatrixMul( world, p, 1.0f, &P[ v * 3 ] );
				if( uv )
					cgltf_accessor_read_float( uv, v, &UV[ v * 2 ], 2 );
			}
			// primitive index list (strips / fans -> list)
			std::vector< uint32_t > idx;
			size_t ni = prim.indices ? prim.indices->count : nv;
			std::vector< uint32_t > raw( ni );
			for( size_t i = 0; i < ni; ++i )
				raw[ i ] = prim.indices ? (uint32_t)cgltf_accessor_read_index( prim.indices, i ) : (uint32_t)i;
			if( prim.type == cgltf_primitive_type_triangles )
				idx = raw;
			else
				for( size_t i = 2; i < ni; ++i )
				{
					if( prim.type == cgltf_primitive_type_triangle_strip )
					{
						bool odd = i & 1;
						idx.insert( idx.end(), { raw[ i - 2 + ( odd ? 1 : 0 ) ], raw[ i - 1 - ( odd ? 1 : 0 ) ], raw[ i ] } );
					}
					else
						idx.insert( idx.end(), { raw[ 0 ], raw[ i - 1 ], raw[ i ] } );
				}
			if( nor )
			{
				for( size_t v = 0; v < nv; ++v )
				{
					float n[ 3 ];
					cgltf_accessor_read_float( nor, v, n, 3 );
					for( int r = 0; r < 3; ++r )
						N[ v * 3 + r ] = nrm[ r * 3 + 0 ] * n[ 0 ] + nrm[ r * 3 + 1 ] * n[ 1 ] + nrm[ r * 3 + 2 ] * n[ 2 ];
					Normalize3( &N[ v * 3 ] );
				}
			}
			else
			{
				// area-weighted smooth normals over the primitive's own vertices
				for( size_t t = 0; t + 2 < idx.size(); t += 3 )
				{
					const float *a = &P[ idx[ t ] * 3 ], *b = &P[ idx[ t + 1 ] * 3 ], *c = &P[ idx[ t + 2 ] * 3 ];
					float e1[ 3 ] = { b[ 0 ] - a[ 0 ], b[ 1 ] - a[ 1 ], b[ 2 ] - a[ 2 ] };
					float e2[ 3 ] = { c[ 0 ] - a[ 0 ], c[ 1 ] - a[ 1 ], c[ 2 ] - a[ 2 ] };
					float fn[ 3 ] = { e1[ 1 ] * e2[ 2 ] - e1[ 2 ] * e2[ 1 ], e1[ 2 ] * e2[ 0 ] - e1[ 0 ] * e2[ 2 ], e1[ 0 ] * e2[ 1 ] - e1[ 1 ] * e2[ 0 ] };
					for( int k = 0; k < 3; ++k )
						for( int r = 0; r < 3; ++r )
							N[ idx[ t + k ] * 3 + r ] += fn[ r ];
				}
				for( size_t v = 0; v < nv; ++v )
					Normalize3( &N[ v * 3 ] );
			}
			if( tan && nor )
				for( size_t v = 0; v < nv; ++v )
				{
					float t4[ 4 ];
					cgltf_accessor_read_float( tan, v, t4, 4 );
					GltfMatrixMul( world, t4, 0.0f, &T[ v * 4 ] );
					Normalize3( &T[ v * 4 ] );
					T[ v * 4 + 3 ] = t4[ 3 ] < 0 ? -1.0f : 1.0f;
				}
			uint32_t slot = slotFor( prim.material );
			for( size_t t = 0; t + 2 < idx.size(); t += 3 )
			{
				Corner c[ 3 ];
				for( int k = 0; k < 3; ++k )
				{
					uint32_t v = idx[ t + k ];
					if( v >= nv )
						v = 0;
					std::memcpy( c[ k ].p, &P[ v * 3 ], 12 );
					std::memcpy( c[ k ].n, &N[ v * 3 ], 12 );
					std::memcpy( c[ k ].uv, &UV[ v * 2 ], 8 );
					std::memcpy( c[ k ].t, &T[ v * 4 ], 16 );
				}
				mb.AddTriangle( c[ 0 ], c[ 1 ], c[ 2 ], slot );
			}
		}
	}
	cgltf_free( data );
	if( cancel.load() )
	{
		error = "cancelled";
		return false;
	}
	return true;
}

// ---------------------------------------------------------------- orientation sidecar
// orient.txt next to the model: "Rotate X <deg>" / "Rotate Y <deg>" / "Rotate Z <deg>" lines ('#' = comment),
// the same dials and order as the Transform group (X first, then Y, then Z). Applied to the geometry before
// auto-fit, so the model's front faces the camera (+Z) and the eye finder, Light C and the fit all see it that way.
// For files whose front is not +Z / up +Y (scans, Z-up exports); the model file itself is never touched.
void ApplyOrientSidecar( const std::string& dir, MeshBuilder& mb )
{
	std::ifstream f( dir + "/orient.txt" );
	if( !f )
		return;
	float deg[ 3 ] = { 0, 0, 0 };
	std::string line;
	while( std::getline( f, line ) )
	{
		line = line.substr( 0, line.find( '#' ) );
		char axis = 0;
		float v   = 0;
		if( sscanf( line.c_str(), " %*[Rr]otate %c %f", &axis, &v ) == 2 )
		{
			int k = ( axis | 32 ) - 'x';
			if( k >= 0 && k < 3 )
				deg[ k ] = v;
		}
	}
	if( deg[ 0 ] == 0 && deg[ 1 ] == 0 && deg[ 2 ] == 0 )
		return;
	// R = Rz * Ry * Rx (row-major 3x3)
	const float d2r = 3.14159265358979f / 180.0f;
	float cx = std::cos( deg[ 0 ] * d2r ), sx = std::sin( deg[ 0 ] * d2r );
	float cy = std::cos( deg[ 1 ] * d2r ), sy = std::sin( deg[ 1 ] * d2r );
	float cz = std::cos( deg[ 2 ] * d2r ), sz = std::sin( deg[ 2 ] * d2r );
	const float R[ 9 ] = { cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
		                   sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
		                   -sy, cy * sx, cy * cx };
	auto rot = []( const float* M, float* v ) {
		float r[ 3 ];
		for( int i = 0; i < 3; ++i )
			r[ i ] = M[ i * 3 ] * v[ 0 ] + M[ i * 3 + 1 ] * v[ 1 ] + M[ i * 3 + 2 ] * v[ 2 ];
		std::memcpy( v, r, sizeof r );
	};
	for( Corner& c : mb.corners )
		rot( R, c.p ), rot( R, c.n ), rot( R, c.t );
	Log( "  orient.txt: Rotate X %g, Y %g, Z %g applied to the geometry", deg[ 0 ], deg[ 1 ], deg[ 2 ] );
}

// ---------------------------------------------------------------- parts (W18)
// A part is a (node, material) group. A material used by one node stays one part named after the material (the
// names every earlier wave saw: a four-material mask keeps its four parts). A material spread over several nodes
// splits into one part per node, named "<node>", or "<node>/<material>" when that node carries several materials, so
// a stand or prop that is its own node gets its own Visible toggle. parts.txt (next to the model, data like
// orient.txt) can split a part's low islands off: "Split <part> below <height 0..1> as <name>". More than
// kMaxParts parts: the smallest multi-node material is merged back into one part until they fit (logged).
bool IsGenericNodeName( const MeshNode& n )
{
	const std::string& s = n.name;
	if( s.empty() )
		return true;
	if( s.compare( 0, 7, "Object_" ) == 0 && s.size() > 7 && std::all_of( s.begin() + 7, s.end(), ::isdigit ) )
		return true;
	// exporter-made mesh nodes: "<parent>_<material>_<n>"
	return !n.parent.empty() && s.size() > n.parent.size() + 1 && s.compare( 0, n.parent.size(), n.parent ) == 0 && s[ n.parent.size() ] == '_';
}

std::string NodeDisplayName( const MeshNode& n, size_t index )
{
	if( IsGenericNodeName( n ) && !n.parent.empty() )
		return n.parent;
	return n.name.empty() ? "node " + std::to_string( index + 1 ) : n.name;
}

struct PartGroup
{
	uint32_t mat;
	std::string name;
	size_t tris = 0;
};

// islands (triangles joined by shared positions) of the triangles in `tris`; returns an island id per entry
std::vector< uint32_t > Islands( const MeshBuilder& mb, const std::vector< uint32_t >& tris, uint32_t& count )
{
	std::unordered_map< uint64_t, uint32_t > posId;
	std::vector< uint32_t > parent;
	auto find = [ & ]( uint32_t a ) {
		while( parent[ a ] != a )
			a = parent[ a ] = parent[ parent[ a ] ];
		return a;
	};
	std::vector< uint32_t > corner( tris.size() * 3 );
	for( size_t t = 0; t < tris.size(); ++t )
		for( int k = 0; k < 3; ++k )
		{
			const Corner& c = mb.corners[ tris[ t ] * 3 + k ];
			const uint64_t h = HashBytes( c.p, sizeof( c.p ) );
			auto it          = posId.emplace( h, (uint32_t)parent.size() );
			if( it.second )
				parent.push_back( (uint32_t)parent.size() );
			corner[ t * 3 + k ] = it.first->second;
		}
	for( size_t t = 0; t < tris.size(); ++t )
	{
		const uint32_t a = find( corner[ t * 3 ] );
		parent[ find( corner[ t * 3 + 1 ] ) ] = a;
		parent[ find( corner[ t * 3 + 2 ] ) ] = a;
	}
	std::unordered_map< uint32_t, uint32_t > id;
	std::vector< uint32_t > out( tris.size() );
	for( size_t t = 0; t < tris.size(); ++t )
		out[ t ] = id.emplace( find( corner[ t * 3 ] ), (uint32_t)id.size() ).first->second;
	count = (uint32_t)id.size();
	return out;
}

void ApplyPartsSidecar( const std::string& dir, MeshBuilder& mb, std::vector< PartGroup >& groups )
{
	std::ifstream f( dir + "/parts.txt" );
	if( !f )
		return;
	float lo = 1e30f, hi = -1e30f;
	for( const Corner& c : mb.corners )
		lo = std::min( lo, c.p[ 1 ] ), hi = std::max( hi, c.p[ 1 ] );
	std::string line;
	while( std::getline( f, line ) )
	{
		line = line.substr( 0, line.find( '#' ) );
		const std::string l = Lower( line );
		const size_t sp = l.find( "split " ), bl = l.rfind( " below " ), as = l.rfind( " as " );
		if( sp == std::string::npos || bl == std::string::npos || as == std::string::npos || !( sp < bl && bl < as ) )
			continue;
		auto trim = []( std::string x ) {
			x.erase( 0, x.find_first_not_of( " \t\r" ) );
			x.erase( x.find_last_not_of( " \t\r" ) + 1 );
			return x;
		};
		const std::string target = trim( line.substr( sp + 6, bl - sp - 6 ) ), name = trim( line.substr( as + 4 ) );
		const float frac = std::strtof( line.substr( bl + 7, as - bl - 7 ).c_str(), nullptr );
		size_t g = 0;
		while( g < groups.size() && Lower( groups[ g ].name ) != Lower( target ) )
			++g;
		if( g == groups.size() || name.empty() )
		{
			Log( "  parts.txt: no part named '%s', line skipped", target.c_str() );
			continue;
		}
		std::vector< uint32_t > tris;
		for( uint32_t t = 0; t < mb.triSlot.size(); ++t )
			if( mb.triSlot[ t ] == g )
				tris.push_back( t );
		uint32_t n = 0;
		const std::vector< uint32_t > island = Islands( mb, tris, n );
		std::vector< float > low( n, 1e30f );
		for( size_t t = 0; t < tris.size(); ++t )
			for( int k = 0; k < 3; ++k )
				low[ island[ t ] ] = std::min( low[ island[ t ] ], mb.corners[ tris[ t ] * 3 + k ].p[ 1 ] );
		const float cut = lo + frac * ( hi - lo );
		size_t moved = 0, movedIslands = 0;
		for( uint32_t i = 0; i < n; ++i )
			movedIslands += low[ i ] < cut;
		if( movedIslands == 0 || movedIslands == n )
		{
			Log( "  parts.txt: '%s' has %u islands, %zu reach below %.2f of the height: not split", target.c_str(), n, movedIslands, frac );
			continue;
		}
		const uint32_t ng = (uint32_t)groups.size();
		groups.push_back( { groups[ g ].mat, name, 0 } );
		for( size_t t = 0; t < tris.size(); ++t )
			if( low[ island[ t ] ] < cut )
				mb.triSlot[ tris[ t ] ] = ng, ++moved;
		groups[ g ].tris -= moved;
		groups[ ng ].tris = moved;
		Log( "  parts.txt: %zu of %u islands of '%s' reach below %.2f of the height -> part '%s' (%zu tris)", movedIslands, n, target.c_str(), frac, name.c_str(), moved );
	}
}

// Regroups mb.triSlot from material slots to parts; returns the parts (factors / textures copied from the material).
std::vector< Part > GroupParts( MeshBuilder& mb, const std::vector< MaterialSlot >& slots, const std::string& dir, std::vector< std::string >& slotNames )
{
	const size_t nm = slots.size(), nn = mb.nodes.size();
	std::vector< std::vector< uint32_t > > matNodes( nm ); // nodes per material, in order of appearance
	std::vector< int > nodeMats( nn, 0 );
	std::map< std::pair< uint32_t, uint32_t >, size_t > tris;
	for( size_t t = 0; t < mb.triSlot.size(); ++t )
	{
		auto key = std::make_pair( mb.triSlot[ t ], mb.triNode[ t ] );
		if( tris[ key ]++ == 0 )
		{
			matNodes[ key.first ].push_back( key.second );
			nodeMats[ key.second ]++;
		}
	}
	std::vector< PartGroup > groups;
	std::map< std::pair< uint32_t, uint32_t >, uint32_t > groupOf;
	std::map< std::string, int > used;
	auto unique = [ & ]( std::string n ) {
		const int k = used[ Lower( n ) ]++;
		return k == 0 ? n : n + " " + std::to_string( k + 1 );
	};
	for( uint32_t m = 0; m < nm; ++m )
	{
		if( matNodes[ m ].empty() )
			continue;
		if( matNodes[ m ].size() == 1 )
		{
			groupOf[ { m, matNodes[ m ][ 0 ] } ] = (uint32_t)groups.size();
			groups.push_back( { m, unique( slots[ m ].name ), tris[ { m, matNodes[ m ][ 0 ] } ] } );
			continue;
		}
		// a parent shared by these nodes names none of them: then each node keeps its own name
		std::map< std::string, int > parentUse;
		for( uint32_t n : matNodes[ m ] )
			parentUse[ NodeDisplayName( mb.nodes[ n ], n ) ]++;
		for( uint32_t n : matNodes[ m ] )
		{
			std::string name = NodeDisplayName( mb.nodes[ n ], n );
			if( parentUse[ name ] > 1 && !mb.nodes[ n ].name.empty() )
				name = mb.nodes[ n ].name;
			if( nodeMats[ n ] > 1 )
				name += "/" + slots[ m ].name;
			groupOf[ { m, n } ] = (uint32_t)groups.size();
			groups.push_back( { m, unique( name ), tris[ { m, n } ] } );
		}
		Log( "  parts: material '%s' is used by %zu nodes: one part per node", slots[ m ].name.c_str(), matNodes[ m ].size() );
	}
	for( size_t t = 0; t < mb.triSlot.size(); ++t )
		mb.triSlot[ t ] = groupOf[ { mb.triSlot[ t ], mb.triNode[ t ] } ];
	std::vector< uint32_t >().swap( mb.triNode );

	ApplyPartsSidecar( dir, mb, groups );

	// too many parts: fold the material with the smallest part back into one part, until they fit
	while( groups.size() > (size_t)kMaxParts )
	{
		std::vector< int > perMat( nm, 0 );
		for( const PartGroup& g : groups )
			perMat[ g.mat ]++;
		int pick = -1;
		size_t smallest = SIZE_MAX;
		for( const PartGroup& g : groups )
			if( perMat[ g.mat ] > 1 && g.tris < smallest )
				smallest = g.tris, pick = (int)g.mat;
		if( pick < 0 )
			break;
		std::vector< uint32_t > remap( groups.size() );
		std::vector< PartGroup > kept;
		int into = -1;
		for( size_t g = 0; g < groups.size(); ++g )
		{
			if( groups[ g ].mat == (uint32_t)pick )
			{
				if( into < 0 )
				{
					into = (int)kept.size();
					kept.push_back( { (uint32_t)pick, slots[ pick ].name, 0 } );
				}
				kept[ into ].tris += groups[ g ].tris;
				remap[ g ] = (uint32_t)into;
			}
			else
			{
				remap[ g ] = (uint32_t)kept.size();
				kept.push_back( groups[ g ] );
			}
		}
		Log( "  parts: %zu parts > %d: the %d parts of material '%s' merged into one", groups.size(), kMaxParts, perMat[ pick ], slots[ pick ].name.c_str() );
		groups.swap( kept );
		for( uint32_t& t : mb.triSlot )
			t = remap[ t ];
	}
	if( groups.size() > (size_t)kMaxParts )
		Log( "  parts: %zu materials > %d slots: parts %d.. share the last slot", groups.size(), kMaxParts, kMaxParts );

	std::vector< Part > parts;
	slotNames.clear();
	for( size_t g = 0; g < groups.size(); ++g )
	{
		parts.push_back( slots[ groups[ g ].mat ].part );
		parts.back().name = groups[ g ].name;
		parts.back().slot = std::min( (int)g, kMaxParts - 1 );
		if( g < (size_t)kMaxParts )
			slotNames.push_back( groups[ g ].name );
	}
	return parts;
}

// ---------------------------------------------------------------- decode + mips
// 2x2 box filter down to 1x1; colour channels of sRGB images are averaged in linear light.
void BuildMips( Image& img )
{
	static float toLin[ 256 ];
	static uint8_t toSrgb[ 4096 ];
	static bool init = [] {
		for( int i = 0; i < 256; ++i )
		{
			float c    = i / 255.0f;
			toLin[ i ] = c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f );
		}
		for( int i = 0; i < 4096; ++i )
		{
			float l     = i / 4095.0f;
			float c     = l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow( l, 1.0f / 2.4f ) - 0.055f;
			toSrgb[ i ] = (uint8_t)std::lround( std::min( 1.0f, std::max( 0.0f, c ) ) * 255.0f );
		}
		return true;
	}();
	(void)init;
	const int nc = img.components;
	int w = img.width, h = img.height;
	const uint8_t* src = img.pixels.data();
	img.mips.clear();
	while( w > 1 || h > 1 )
	{
		int nw = std::max( 1, w / 2 ), nh = std::max( 1, h / 2 );
		std::vector< uint8_t > dst( size_t( nw ) * nh * nc );
		for( int y = 0; y < nh; ++y )
		{
			int y0 = std::min( h - 1, y * 2 ), y1 = std::min( h - 1, y * 2 + 1 );
			for( int x = 0; x < nw; ++x )
			{
				int x0 = std::min( w - 1, x * 2 ), x1 = std::min( w - 1, x * 2 + 1 );
				const uint8_t* p[ 4 ] = { &src[ ( size_t( y0 ) * w + x0 ) * nc ], &src[ ( size_t( y0 ) * w + x1 ) * nc ],
					                      &src[ ( size_t( y1 ) * w + x0 ) * nc ], &src[ ( size_t( y1 ) * w + x1 ) * nc ] };
				uint8_t* d = &dst[ ( size_t( y ) * nw + x ) * nc ];
				for( int c = 0; c < nc; ++c )
				{
					if( img.srgb && c < 3 )
					{
						float l = ( toLin[ p[ 0 ][ c ] ] + toLin[ p[ 1 ][ c ] ] + toLin[ p[ 2 ][ c ] ] + toLin[ p[ 3 ][ c ] ] ) * 0.25f;
						d[ c ]  = toSrgb[ std::min( 4095, int( l * 4095.0f + 0.5f ) ) ];
					}
					else
						d[ c ] = uint8_t( ( p[ 0 ][ c ] + p[ 1 ][ c ] + p[ 2 ][ c ] + p[ 3 ][ c ] + 2 ) >> 2 );
				}
			}
		}
		img.mips.push_back( std::move( dst ) );
		src = img.mips.back().data();
		w = nw, h = nh;
	}
}

void DecodeImages( ImageTable& images, int maxTexture, const std::atomic< bool >& cancel )
{
	std::atomic< size_t > next{ 0 };
	auto worker = [ & ]() {
		for( ;; )
		{
			size_t i = next.fetch_add( 1 );
			if( i >= images.sources.size() || cancel.load() )
				return;
			ImageSource& s = *images.sources[ i ];
			int w = 0, h = 0, comp = 0;
			stbi_uc* px = nullptr;
			if( !s.bytes.empty() )
				px = stbi_load_from_memory( s.bytes.data(), (int)s.bytes.size(), &w, &h, &comp, s.components );
			else if( !s.path.empty() )
				px = stbi_load( s.path.c_str(), &w, &h, &comp, s.components );
			if( !px )
			{
				Log( "  decode failed for %s: %s", s.key.c_str(), stbi_failure_reason() );
				continue;
			}
			s.image->width      = w;
			s.image->height     = h;
			s.image->components = s.components;
			s.image->srgb       = s.srgb;
			s.image->pixels.assign( px, px + size_t( w ) * h * s.components );
			stbi_image_free( px );
			if( s.fromSpecGloss )
				for( size_t i = 0; i < s.image->pixels.size(); i += 4 )
				{
					uint8_t* q = &s.image->pixels[ i ];
					int spec   = std::max( q[ 0 ], std::max( q[ 1 ], q[ 2 ] ) );
					float r    = 1.0f - s.glossFactor * q[ 3 ] / 255.0f;
					float mtl  = std::clamp( ( spec / 255.0f - 0.04f ) / 0.46f, 0.0f, 1.0f );
					q[ 0 ] = uint8_t( std::lround( r * 255.0f ) ), q[ 1 ] = uint8_t( std::lround( mtl * 255.0f ) ), q[ 2 ] = 0, q[ 3 ] = 255;
				}
			std::vector< uint8_t >().swap( s.bytes );
			BuildMips( *s.image );
			// Max Texture Size: promote the first mip level that fits to level 0
			Image& im = *s.image;
			int drop  = 0;
			while( maxTexture > 0 && std::max( im.width, im.height ) > maxTexture && drop < (int)im.mips.size() )
			{
				im.width = std::max( 1, im.width / 2 ), im.height = std::max( 1, im.height / 2 );
				drop++;
			}
			if( drop > 0 )
			{
				im.pixels = std::move( im.mips[ drop - 1 ] );
				im.mips.erase( im.mips.begin(), im.mips.begin() + drop );
				Log( "  %s: %dx%d downsampled to %dx%d (Max Texture Size)", s.key.c_str(), w, h, im.width, im.height );
			}
		}
	};
	unsigned n = std::max( 1u, std::min( 8u, std::thread::hardware_concurrency() ) );
	n          = std::min< unsigned >( n, (unsigned)std::max< size_t >( 1, images.sources.size() ) );
	std::vector< std::thread > threads;
	for( unsigned i = 0; i < n; ++i )
		threads.emplace_back( worker );
	for( auto& t : threads )
		t.join();
}
} // namespace

bool IsSupportedModelFile( const std::string& path )
{
	std::string e = Extension( path );
	return e == "fbx" || e == "obj" || e == "gltf" || e == "glb";
}

std::unique_ptr< ModelData > LoadModel( const std::string& path, int maxTexture, const std::atomic< bool >& cancel, std::string& error )
{
	auto model  = std::make_unique< ModelData >();
	model->path = path;
	if( !IsFile( path ) )
	{
		error = "file not found: " + path;
		Log( "LOAD FAIL %s", error.c_str() );
		return nullptr;
	}
	if( !IsSupportedModelFile( path ) )
	{
		error = "unsupported extension: " + path;
		Log( "LOAD FAIL %s", error.c_str() );
		return nullptr;
	}
	Log( "LOAD start %s", path.c_str() );
	FolderIndex folder;
	folder.Add( DirName( path ), 0 );

	MeshBuilder mb;
	std::vector< MaterialSlot > slots;
	ImageTable images;
	auto t0            = Clock::now();
	std::string ext    = Extension( path );
	bool ok            = ( ext == "gltf" || ext == "glb" ) ? LoadGltf( path, cancel, mb, slots, images, error )
	                                                       : LoadUfbx( path, cancel, mb, slots, images, folder, error );
	model->msParse = MsSince( t0 );
	if( !ok )
	{
		Log( "LOAD FAIL %s: %s", path.c_str(), error.c_str() );
		return nullptr;
	}
	if( mb.triSlot.empty() )
	{
		error = "no triangles in file";
		Log( "LOAD FAIL %s: %s", path.c_str(), error.c_str() );
		return nullptr;
	}

	// sidecars fill every channel the file left empty
	for( MaterialSlot& s : slots )
	{
		auto names = MaterialNameVariants( s.name );
		for( const Alias& a : kAliases )
		{
			if( s.fileTex[ a.channel ] || s.part.tex[ a.channel ].image )
				continue;
			for( int i = 0; a.names[ i ]; ++i )
			{
				bool exr      = false;
				std::string p = FindSidecar( folder, names, a.names[ i ], exr );
				if( exr )
					Log( "  [%s] %s: .exr sidecar found but EXR is not supported, skipped", s.name.c_str(), ChannelName( a.channel ) );
				if( p.empty() )
					continue;
				ImageSource* src                  = images.Get( p, ComponentsFor( a.channel ), IsColorChannel( a.channel ) );
				src->path                         = p;
				s.part.tex[ a.channel ].image     = src->image;
				s.part.tex[ a.channel ].flipGreen = a.dx;
				s.part.tex[ a.channel ].source    = "sidecar " + BaseName( p );
				break;
			}
		}
		// packed ORM sidecar (R=AO G=Rough B=Metal) for channels still empty
		const char* ormNames[] = { "orm", "occlusionroughnessmetallic" };
		for( const char* on : ormNames )
		{
			bool exr      = false;
			std::string p = FindSidecar( folder, names, on, exr );
			if( p.empty() )
				continue;
			ImageSource* src = images.Get( p, 4 );
			src->path        = p;
			const int chs[ 3 ] = { CH_AO, CH_ROUGHNESS, CH_METALLIC };
			for( int k = 0; k < 3; ++k )
				if( !s.fileTex[ chs[ k ] ] && !s.part.tex[ chs[ k ] ].image )
				{
					s.part.tex[ chs[ k ] ].image     = src->image;
					s.part.tex[ chs[ k ] ].component = k;
					s.part.tex[ chs[ k ] ].source    = std::string( "sidecar ORM " ) + BaseName( p );
				}
			break;
		}
		// file-referenced textures win
		for( int ch = 0; ch < CH_COUNT; ++ch )
			if( s.fileTex[ ch ] )
			{
				s.part.tex[ ch ].image     = s.fileTex[ ch ]->image;
				s.part.tex[ ch ].component = s.fileComponent[ ch ];
				s.part.tex[ ch ].source    = "file " + s.fileTex[ ch ]->key;
			}
		// FBX/OBJ: a texture replaces the factor. glTF: factor multiplies the texture.
		if( !s.factorsMultiplyTextures )
		{
			if( s.part.tex[ CH_BASECOLOR ].image )
				s.part.baseColor[ 0 ] = s.part.baseColor[ 1 ] = s.part.baseColor[ 2 ] = 1.0f;
			if( s.part.tex[ CH_ROUGHNESS ].image )
				s.part.roughness = 1.0f;
			if( s.part.tex[ CH_METALLIC ].image )
				s.part.metallic = 1.0f;
			if( s.part.tex[ CH_EMISSIVE ].image )
				s.part.emissive[ 0 ] = s.part.emissive[ 1 ] = s.part.emissive[ 2 ] = 1.0f;
		}
	}

	auto t1 = Clock::now();
	ApplyOrientSidecar( DirName( path ), mb );
	std::vector< Part > parts = GroupParts( mb, slots, DirName( path ), model->slotNames );
	FinalizeGeometry( mb, parts, *model );
	{
		MeshBuilder empty;
		std::swap( mb, empty );
	}
	model->msGeometry = MsSince( t1 );
	if( cancel.load() )
	{
		error = "cancelled";
		return nullptr;
	}

	auto t2 = Clock::now();
	DecodeImages( images, maxTexture, cancel );
	model->msDecode = MsSince( t2 );
	if( cancel.load() )
	{
		error = "cancelled";
		return nullptr;
	}

	for( Part& p : parts )
	{
		for( int ch = 0; ch < CH_COUNT; ++ch )
		{
			TextureRef& t = p.tex[ ch ];
			if( t.image && !t.image->Valid() )
				t = TextureRef();
			Log( "  part '%s' %-9s : %s", p.name.c_str(), ChannelName( ch ), t.image ? t.source.c_str() : "(factor / default)" );
		}
		if( p.glass > 0.0f )
		{
			// glass tint = base colour factor x the map's average colour (its 1 x 1 mip), linear
			float avg[ 3 ] = { 1, 1, 1 };
			if( const Image* img = p.tex[ CH_BASECOLOR ].image.get() )
			{
				const std::vector< uint8_t >& px = img->mips.empty() ? img->pixels : img->mips.back();
				for( int k = 0; k < 3; ++k )
				{
					const float c = px[ std::min( k, img->components - 1 ) ] / 255.0f;
					avg[ k ]      = img->srgb ? ( c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f ) ) : c;
				}
			}
			for( int k = 0; k < 3; ++k )
				p.glassTint[ k ] = p.baseColor[ k ] * avg[ k ];
			Log( "  part '%s' drawn as glass: amount %.2f, tint (%.2f %.2f %.2f)", p.name.c_str(), p.glass, p.glassTint[ 0 ], p.glassTint[ 1 ], p.glassTint[ 2 ] );
		}
		Log( "  part '%s' slot %d: %u tris, bbox (%.4g %.4g %.4g) .. (%.4g %.4g %.4g)", p.name.c_str(), p.slot + 1, p.indexCount / 3, p.boxMin[ 0 ], p.boxMin[ 1 ],
		     p.boxMin[ 2 ], p.boxMax[ 0 ], p.boxMax[ 1 ], p.boxMax[ 2 ] );
		model->parts.push_back( std::move( p ) );
	}
	Log( "  bbox (%.4g %.4g %.4g) .. (%.4g %.4g %.4g), fit scale %.4g", model->bboxMin[ 0 ], model->bboxMin[ 1 ], model->bboxMin[ 2 ],
	     model->bboxMax[ 0 ], model->bboxMax[ 1 ], model->bboxMax[ 2 ], model->fitScale );
	Log( "LOAD done %s: %zu tris, %zu verts, %zu parts, %zu images | parse %.0f ms, geometry %.0f ms, decode %.0f ms",
	     path.c_str(), model->triangleCount, model->vertices.size(), model->parts.size(), images.sources.size(),
	     model->msParse, model->msGeometry, model->msDecode );
	return model;
}

void BuildImageMips( Image& img )
{
	BuildMips( img );
}

std::unique_ptr< ModelData > ModelFromMesh( GenMesh& g, const std::string& path )
{
	auto model  = std::make_unique< ModelData >();
	model->path = path;
	MeshBuilder mb;
	mb.allHaveTangents = false;
	const size_t tris  = g.corners.size() / 3;
	mb.corners.resize( g.corners.size() );
	for( size_t i = 0; i < g.corners.size(); ++i )
	{
		Corner& c = mb.corners[ i ];
		std::memcpy( c.p, g.corners[ i ].p, sizeof( c.p ) );
		std::memcpy( c.n, g.corners[ i ].n, sizeof( c.n ) );
		std::memcpy( c.uv, g.corners[ i ].uv, sizeof( c.uv ) );
		c.t[ 0 ] = c.t[ 1 ] = c.t[ 2 ] = 0.0f, c.t[ 3 ] = 1.0f;
	}
	mb.triSlot = g.triPart;
	mb.triNode.assign( tris, 0 );
	for( size_t p = 0; p < g.parts.size(); ++p )
	{
		g.parts[ p ].slot = int( p );
		model->slotNames.push_back( g.parts[ p ].name );
	}
	FinalizeGeometry( mb, g.parts, *model );
	// FinalizeGeometry sorts the triangles by part (stable): the groups follow the same order
	std::vector< uint32_t > cursor( g.parts.size(), 0 ), start( g.parts.size(), 0 );
	for( uint32_t t : g.triPart )
		cursor[ t ]++;
	for( size_t p = 1; p < g.parts.size(); ++p )
		start[ p ] = start[ p - 1 ] + cursor[ p - 1 ];
	model->triGroup.resize( tris );
	for( size_t t = 0; t < tris; ++t )
		model->triGroup[ start[ g.triPart[ t ] ]++ ] = (uint16_t)std::min( g.triGroup[ t ], 65535u );
	model->groupCount = g.groupCount;
	for( Part& p : g.parts )
		model->parts.push_back( std::move( p ) );
	return model;
}
}
