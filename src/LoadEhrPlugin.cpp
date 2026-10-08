// LoadEhr — load any 3D model (fbx / obj / gltf / glb) into Resolume and animate it live.
// Built twice from this file: LOADEHR_FX=0 -> source "LoadEhr", LOADEHR_FX=1 -> effect "LoadEhr FX".
#include "FFGLPluginSDK.h"
#include "FFGLLog.h"
#include "EyeFinder.h"
#include "Generate.h"
#include "SdfBuilder.h"
#include "GpuModel.h"
#include "LoadJob.h"
#include "Log.h"
#include "MathUtil.h"
#include "Params.h"
#include "Renderer.h"
#if LOADEHR_FX
#include "FaceTracker.h"
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <thread>
#include "stb_image.h"
#include <string>
#include <vector>

using namespace mehr;

namespace
{
constexpr float kDeg = 3.14159265f / 180.0f;

struct ParamDef
{
	unsigned id;
	const char* name;
	unsigned type;
	float min, max, def;
	const char* group;
	std::vector< std::string > options; // FF_TYPE_OPTION element names (values 0..n-1)
	bool hidden;
	int region; // MEHR_REGION: 0 every bundle, MEHR_WOW / MEHR_CLASSIC one family only
};

std::vector< std::string > SplitOptions( const char* s )
{
	std::vector< std::string > out;
	std::string cur;
	for( ; *s; ++s )
		if( *s == '|' )
			out.push_back( cur ), cur.clear();
		else
			cur += *s;
	if( !cur.empty() )
		out.push_back( cur );
	return out;
}

const std::vector< ParamDef >& ParamDefs()
{
	enum : unsigned
	{
		FILE   = FF_TYPE_FILE,
		TEXT   = FF_TYPE_TEXT,
		OPT    = FF_TYPE_OPTION,
		EVENT  = FF_TYPE_EVENT,
		BOOL   = FF_TYPE_BOOLEAN,
		STD    = FF_TYPE_STANDARD,
		HUE    = FF_TYPE_HUE,
		SAT    = FF_TYPE_SATURATION,
		BRI    = FF_TYPE_BRIGHTNESS,
		ALPHA  = FF_TYPE_ALPHA,
		BUFFER = FF_TYPE_BUFFER,
		HIDDEN = 1
	};
	static const std::vector< ParamDef > defs = {
#define X( id, name, type, mn, mx, def, group, opts, flags ) { id, name, type, mn, mx, def, group, SplitOptions( opts ), ( flags ) == HIDDEN, MEHR_REGION },
#include "ParamList.inc"
#undef X
	};
	return defs;
}

// W26: which params this bundle carries (Resolume shows at most 512). Every id exists in every build; the host sees the
// carried ones as FFGL indices 0..n-1 in id order. ffgl[ id ] = its index (kNoParam = not carried), id[ index ] = back.
constexpr unsigned kNoParam = ~0u;
struct ParamMap
{
	std::vector< unsigned > ffgl, id;
};
const ParamMap& Map()
{
	static const ParamMap m = [] {
		ParamMap r;
		r.ffgl.assign( P_COUNT, kNoParam );
		for( const ParamDef& p : ParamDefs() )
			if( p.region == 0 || p.region == ( LOADEHR_WOW ? MEHR_WOW : MEHR_CLASSIC ) )
			{
				r.ffgl[ p.id ] = (unsigned)r.id.size();
				r.id.push_back( p.id );
			}
		return r;
	}();
	return m;
}
unsigned IdOf( unsigned index ) { return index < Map().id.size() ? Map().id[ index ] : P_COUNT; }

// Beat envelope with a short smooth attack and a decay that reaches zero at the next beat,
// so it is continuous across beat boundaries.
float BeatPulse( float phase )
{
	float s = std::min( 1.0f, std::max( 0.0f, phase / 0.1f ) );
	s       = s * s * ( 3.0f - 2.0f * s );
	float d = 1.0f - phase;
	return s * d * d * d * d;
}

// Arena shows 16 characters of a parameter name: "<prefix> <field>" with the prefix cut to fit.
constexpr size_t kMaxNameLen = 16;
std::string CapName( std::string prefix, const std::string& suffix, const std::string& field )
{
	const size_t room = kMaxNameLen - std::min( kMaxNameLen, field.size() + 1 + suffix.size() );
	if( prefix.size() > room )
		prefix.resize( room );
	while( !prefix.empty() && prefix.back() == ' ' )
		prefix.pop_back();
	prefix += suffix;
	return prefix.empty() ? field : prefix + " " + field;
}

// Arena's picker hands over hue / saturation / brightness in 0..1. Hue 1.0 is red again (wrapped here:
// the SDK's HSVtoRGB returns magenta at exactly 1.0).
void HsbToRgb( float h, float s, float v, float* rgb )
{
	h           = h - std::floor( h );
	const float x = h * 6.0f;
	const int i   = std::min( 5, (int)x );
	const float f = x - float( i );
	const float p = v * ( 1.0f - s ), q = v * ( 1.0f - s * f ), t = v * ( 1.0f - s * ( 1.0f - f ) );
	const float table[ 6 ][ 3 ] = { { v, t, p }, { q, v, p }, { p, v, t }, { p, q, v }, { t, p, v }, { v, p, q } };
	for( int k = 0; k < 3; ++k )
		rgb[ k ] = table[ i ][ k ];
}

std::string Fmt( const char* fmt, ... ) __attribute__( ( format( printf, 1, 2 ) ) );
std::string Fmt( const char* fmt, ... )
{
	char buf[ 256 ];
	va_list a;
	va_start( a, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, a );
	va_end( a );
	return buf;
}

float SmoothStep( float a, float b, float x )
{
	const float t = std::min( 1.0f, std::max( 0.0f, ( x - a ) / ( b - a ) ) );
	return t * t * ( 3.0f - 2.0f * t );
}

// Periodic in t with period kSmoothPeriod = 200 pi (100, 231 and 473 whole cycles), so a clock feeding it
// wraps seamlessly when every argument it passes moves by a whole number of periods.
constexpr double kSmoothPeriod = 200.0 * 3.14159265358979323846;
float Smooth1D( double t, float seed )
{
	return float( 0.6 * std::sin( t * 1.0 + seed * 1.7 ) + 0.3 * std::sin( t * 2.31 + seed * 4.1 ) + 0.1 * std::sin( t * 4.73 + seed * 2.9 ) );
}
// v wrapped into [0, period)
double Wrap( double v, double period )
{
	v = std::fmod( v, period );
	return v < 0.0 ? v + period : v;
}
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kMoteLoop = 3600.0; // seconds; must match kMoteLoop in VolumeShaders.h
constexpr double kSwarmLoop = 3600.0; // seconds; must match kLoop in kSwarmDrawVS (SwarmShaders.h)
constexpr double kFilmLoop = 256.0;  // film swirl, noise units (0.07 per second: one loop per ~61 min)
constexpr double kPatLoop  = 1000.0; // pattern crawl, pattern units (Pattern Speed per second)
// the cross-fade weight toward the (phase - loop) field over the last 5 % of a loop (smooth, 0 at the wrap's far side)
float LoopFade( double phase, double loop )
{
	const double u = std::min( 1.0, std::max( 0.0, ( phase - 0.95 * loop ) / ( 0.05 * loop ) ) );
	return float( u * u * ( 3.0 - 2.0 * u ) );
}
}

class LoadEhrPlugin : public CFFGLPlugin
{
public:
	LoadEhrPlugin()
	{
		SetMinInputs( LOADEHR_FX ? 1 : 0 );
		SetMaxInputs( LOADEHR_FX ? 1 : 0 );
		SetTimeSupported( true );
		values.assign( P_COUNT, 0.0f );

		for( const ParamDef& p : ParamDefs() )
		{
			values[ p.id ] = p.def;
			if( p.type == FF_TYPE_TEXT )
				texts[ p.id ] = TextDefault( p.id );
			if( !Has( p.id ) )
				continue; // not in this bundle: keeps its default (= off)
			switch( p.type )
			{
			case FF_TYPE_FILE:
				if( p.id == P_FONT_FILE )
					SetFileParamInfo( p.id, p.name, { "ttf", "otf", "ttc" }, "" );
				else if( p.id == P_PROJ_FILE )
					SetFileParamInfo( p.id, p.name, { "png", "jpg", "jpeg", "mov", "mp4", "m4v" }, "" );
				else if( p.id == P_LOGO_FILE )
					SetFileParamInfo( p.id, p.name, { "svg", "pdf", "ai", "png", "jpg", "jpeg", "eps" }, "" );
				else
					SetFileParamInfo( p.id, p.name, { "fbx", "obj", "gltf", "glb" }, "" );
				break;
			case FF_TYPE_TEXT:
				SetParamInfo( p.id, p.name, FF_TYPE_TEXT, TextDefault( p.id ) );
				break;
			case FF_TYPE_BUFFER:
				SetBufferParamInfo( p.id, p.name, kFFTSize, FF_USAGE_FFT );
				break;
			case FF_TYPE_OPTION:
				if( p.id == P_FONT )
				{
					const std::vector< std::string >& fam = Families();
					SetOptionParamInfo( p.id, p.name, (unsigned)fam.size() + 1, 0.0f );
					SetParamElementInfo( p.id, 0, "Use Font Name", 0.0f );
					for( size_t i = 0; i < fam.size(); ++i )
						SetParamElementInfo( p.id, (unsigned)i + 1, fam[ i ].c_str(), float( i + 1 ) );
				}
				else if( p.id == P_FONT_STYLE )
				{
					styles = FontStyles( TextDefault( P_FONT_NAME ) );
					SetOptionParamInfo( p.id, p.name, (unsigned)styles.size() + 1, 0.0f );
					SetParamElementInfo( p.id, 0, "Default", 0.0f );
					for( size_t i = 0; i < styles.size(); ++i )
						SetParamElementInfo( p.id, (unsigned)i + 1, styles[ i ].c_str(), float( i + 1 ) );
				}
				else
				{
					SetOptionParamInfo( p.id, p.name, (unsigned)p.options.size(), p.def );
					for( size_t i = 0; i < p.options.size(); ++i )
						SetParamElementInfo( p.id, (unsigned)i, p.options[ i ].c_str(), float( i ) );
				}
				break;
			case FF_TYPE_BOOLEAN:
			case FF_TYPE_EVENT:
				SetParamInfo( p.id, p.name, p.type, p.def > 0.5f );
				break;
			default:
				SetParamInfo( p.id, p.name, p.type, p.def );
				if( !( p.min == 0.0f && p.max == 1.0f ) )
				{
					// Ranged params carry real values; SetParamInfo clamps defaults to 0..1, undo that.
					SetParamRange( p.id, p.min, p.max );
					FindParamInfo( p.id )->defaultFloatVal = p.def;
				}
				break;
			}
			SetParamGroup( p.id, p.group );
			if( p.hidden )
				SetParamVisibility( p.id, false, false );
		}
		for( unsigned s = 0; s < kMaxParts; ++s )
			for( unsigned f = 0; f < kPartFields; ++f )
				SetParamVisibility( PartParam( s, PartField( f ) ), false, false );
		ParsePoses( "" ); // the four default poses
	}

	~LoadEhrPlugin() override
	{
		Log( "TRIG instance %p destroyed after %llu frames", (void*)this, (unsigned long long)frames );
		if( On( P_DEBUG_LOG ) )
			LogVerboseRef( false );
	}

	FFResult InitGL( const FFGLViewportStruct* vp ) override
	{
		currentViewport = *vp;
		std::string error;
		if( !renderer.Init( error ) )
		{
			Log( "InitGL failed: %s", error.c_str() );
			FFGLLog::LogToHost( ( "LoadEhr: " + error ).c_str() );
			renderer.Release();
			return FF_FAIL;
		}
		Log( "InitGL ok (%s, instance %p, viewport %ux%u)", LOADEHR_FX ? "FX" : "Source", (void*)this, vp->width, vp->height );
		lastClock = std::chrono::steady_clock::now();
		return FF_SUCCESS;
	}

	FFResult DeInitGL() override
	{
		Log( "TRIG DeInitGL instance %p after %llu frames", (void*)this, (unsigned long long)frames );
		for( ModelSlot& m : slots )
			m.pending.reset(), m.current.reset(), m.ticket.reset();
		renderer.Release();
		return FF_SUCCESS;
	}

	// ---- host entry points: FFGL index -> param id
	FFResult SetFloatParameter( unsigned int index, float value ) override { return SetFloatId( IdOf( index ), value ); }
	float GetFloatParameter( unsigned int index ) override { return GetFloatId( IdOf( index ) ); }
	FFResult SetTextParameter( unsigned int index, const char* value ) override { return SetTextId( IdOf( index ), value ); }
	char* GetTextParameter( unsigned int index ) override { return GetTextId( IdOf( index ) ); }
	char* GetParameterDisplay( unsigned int index ) override { return GetDisplayId( IdOf( index ) ); }

	// ---- the SDK's param calls by id (a param this bundle does not carry is skipped)
	static bool Has( unsigned id ) { return id < P_COUNT && Map().ffgl[ id ] != kNoParam; }
	static unsigned Ff( unsigned id ) { return Map().ffgl[ id ]; }
	template< class... A > void SetParamInfo( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamInfo( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetBufferParamInfo( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetBufferParamInfo( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetOptionParamInfo( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetOptionParamInfo( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetParamElementInfo( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamElementInfo( Ff( id ), std::forward< A >( a )... ); }
	void SetFileParamInfo( unsigned id, const char* name, std::vector< std::string > ext, const char* def ) { if( Has( id ) ) CFFGLPlugin::SetFileParamInfo( Ff( id ), name, ext, def ); }
	template< class... A > void SetParamVisibility( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamVisibility( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetParamRange( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamRange( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetParamGroup( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamGroup( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetParamDisplayName( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamDisplayName( Ff( id ), std::forward< A >( a )... ); }
	template< class... A > void SetParamElements( unsigned id, A&&... a ) { if( Has( id ) ) CFFGLPlugin::SetParamElements( Ff( id ), std::forward< A >( a )... ); }
	void RaiseParamEvent( unsigned id, FFUInt64 e ) { if( Has( id ) ) CFFGLPlugin::RaiseParamEvent( Ff( id ), e ); }
	ParamInfo* FindParamInfo( unsigned id ) { return Has( id ) ? CFFGLPlugin::FindParamInfo( Ff( id ) ) : nullptr; }
	unsigned GetParamType( unsigned id ) const { return id < P_COUNT ? ParamDefs()[ id ].type : FF_TYPE_STANDARD; }
	char* GetParamElementName( unsigned id, unsigned el ) { return Has( id ) ? CFFGLPlugin::GetParamElementName( Ff( id ), el ) : nullptr; }

	FFResult SetFloatId( unsigned int index, float value )
	{
		if( index >= P_COUNT || ParamDefs()[ index ].type == FF_TYPE_FILE || ParamDefs()[ index ].type == FF_TYPE_TEXT )
			return FF_FAIL;
		++setCalls, setChanged += value != values[ index ] ? 1 : 0; // W28: the host's param bursts (clip load / trigger?)
		for( int i = 0; i < kModelSlots; ++i )
			if( index == SlotParam( i, SF_RELOAD ) && value > 0.5f && values[ index ] <= 0.5f )
				reloadRequested[ i ] = true; // re-read that slot's file from disk (no folder is ever scanned)
		if( index == P_RESET_SPIN && value > 0.5f && values[ index ] <= 0.5f )
			clk.spin[ 0 ] = clk.spin[ 1 ] = clk.spin[ 2 ] = 0.0; // deliberate jump: that is what the button is for
		const bool press = value > 0.5f && values[ index ] <= 0.5f;
		if( index == P_SHATTER && press )
			shatterRequest = 1;
		if( index == P_REFORM && press )
			shatterRequest = 2;
		if( index == P_BLOW_DUST && press )
			dustPress = 1;
		if( index == P_DUST_REFORM && press )
			dustPress = 2;
		if( index == P_DUST && value != values[ index ] )
			dustManual = true; // the slider scrubs the dust by hand until the next Blow To Dust / Dust Reform
		if( index == P_TUMBLE_RESET && press )
			tumbleResetT = 0.0f;
		if( index >= P_STORE_POSE1 && index <= P_STORE_POSE4 && press )
			StorePose( int( index - P_STORE_POSE1 ) );
		if( index >= P_GO_POSE1 && index <= P_GO_POSE4 && press )
			GoPose( int( index - P_GO_POSE1 ) );
		if( index == P_LC_PLACE && press )
			eyeRequest = EyePlace;
		if( index == P_SIM_RESET && press )
			simResetRequest = true;
		if( index == P_CRACK_START && press )
			crackPress = 1; // W26 lava
		if( index == P_CRACK_REVERSE && press )
			crackPress = 2;
		if( index == P_CRACK_SPREAD && value != values[ index ] )
			crackManual = true;
		if( index == P_VIDEO_RESTART && press )
			videoRestart = true; // W2
		if( index == P_VIDEO_POS && value != values[ index ] && !PushEcho( value ) )
			videoCue = std::clamp( value, 0.0f, 1.0f ); // W4: a user change (the latest wins); in Loop / Ping-Pong / Once it cues
		if( index == P_SWARM_RELEASE && press )
			swarmRelease = true; // W26
		if( index == P_STRIKE && press )
			strikeRequest = true; // W26 lightning
		if( index == P_FIRE_RING && press )
			ringFire = true; // W24: a vortex ring now
		if( index == P_LC_FIT_CONE && press )
			eyeRequest = EyeFitCone;
		if( index == P_FONT )
		{
			// a Font pick writes Font Name (the comp stores the NAME, never the installed list's order)
			const int f = (int)std::lround( value );
			values[ P_FONT ] = value;
			if( f > 0 && f <= (int)Families().size() && Families()[ f - 1 ] != texts[ P_FONT_NAME ] )
			{
				SetFontName( Families()[ f - 1 ] );
				RaiseParamEvent( P_FONT_NAME, FF_EVENT_FLAG_VALUE );
				values[ P_FONT_STYLE ] = 0.0f; // a new family starts at its default face
				RaiseParamEvent( P_FONT_STYLE, FF_EVENT_FLAG_VALUE );
			}
			return FF_SUCCESS;
		}
		if( index == P_DEBUG_LOG && ( value > 0.5f ) != ( values[ index ] > 0.5f ) )
			LogVerboseRef( value > 0.5f );
		if( values[ index ] != value && LogVerbose() )
			Log( "param %u '%s' (type %u) <- %.4f", index, ParamDefs()[ index ].name, GetParamType( index ), value );
		values[ index ] = value;
		return FF_SUCCESS;
	}

	float GetFloatId( unsigned int index )
	{
		return index < P_COUNT ? values[ index ] : 0.0f;
	}

	FFResult SetTextId( unsigned int index, const char* value )
	{
		const std::string v = value ? value : "";
		if( SlotOfParam( index, SF_FILE ) >= 0 )
			SetModelFile( SlotOfParam( index, SF_FILE ), v );
		else if( SlotOfParam( index, SF_STATUS ) >= 0 || index == P_VIDEO_TIME )
			RaiseParamEvent( index, FF_EVENT_FLAG_VALUE ); // read-only: answer with the real status / video time
		else if( index == P_POSE_DATA )
			ParsePoses( v );
		else if( index == P_LC_PLACED )
			eyePlacedFor = v;
		else if( index == P_FONT_NAME )
			SetFontName( v );
		else if( texts.count( index ) || ( index < P_COUNT && ParamDefs()[ index ].type == FF_TYPE_FILE ) )
		{
			if( texts[ index ] != v && ( LogVerbose() || index != P_TEXT ) )
				Log( "%s <- '%s'", ParamDefs()[ index ].name, v.c_str() );
			texts[ index ] = v;
		}
		else
			return FF_FAIL;
		return FF_SUCCESS;
	}

	char* GetTextId( unsigned int index )
	{
		if( SlotOfParam( index, SF_FILE ) >= 0 )
			return (char*)modelParam[ SlotOfParam( index, SF_FILE ) ].c_str();
		if( SlotOfParam( index, SF_STATUS ) >= 0 )
			return (char*)status[ SlotOfParam( index, SF_STATUS ) ].c_str();
		if( index == P_POSE_DATA )
			return (char*)poseData.c_str();
		if( index == P_LC_PLACED )
			return (char*)eyePlacedFor.c_str();
		auto it = texts.find( index );
		return it == texts.end() ? (char*)"" : (char*)it->second.c_str();
	}

	char* GetDisplayId( unsigned int index )
	{
		if( SlotOfParam( index, SF_FILE ) >= 0 || index == P_FONT_FILE || index == P_LOGO_FILE || index == P_PROJ_FILE )
		{
			const std::string& f = SlotOfParam( index, SF_FILE ) >= 0 ? modelParam[ SlotOfParam( index, SF_FILE ) ] : texts[ index ];
			size_t s             = f.find_last_of( '/' );
			display              = s == std::string::npos ? f : f.substr( s + 1 );
			return (char*)display.c_str();
		}
		if( index == P_POSE_DATA )
			return (char*)poseData.c_str();
		if( index == P_LC_PLACED )
			return (char*)eyePlacedFor.c_str();
		if( SlotOfParam( index, SF_STATUS ) >= 0 )
			return (char*)status[ SlotOfParam( index, SF_STATUS ) ].c_str();
		if( texts.count( index ) )
			return (char*)texts[ index ].c_str();
		if( index >= P_COUNT )
			return (char*)"";
		char buf[ 64 ];
		unsigned type = GetParamType( index );
		if( type == FF_TYPE_OPTION )
		{
			char* name = GetParamElementName( index, (unsigned)Opt( index ) );
			return name ? name : (char*)"";
		}
		if( type == FF_TYPE_BOOLEAN || type == FF_TYPE_EVENT )
			snprintf( buf, sizeof( buf ), "%s", values[ index ] > 0.5f ? "On" : "Off" );
		else
			snprintf( buf, sizeof( buf ), "%.2f", values[ index ] );
		display = buf;
		return (char*)display.c_str();
	}

	FFResult ProcessOpenGL( ProcessOpenGLStruct* gl ) override
	{
		auto now      = std::chrono::steady_clock::now();
		float dtClock = std::chrono::duration< float >( now - lastClock ).count();
		lastClock     = now;
		float dt      = dtClock;
		if( hostTimeValid )
		{
			// W28: Arena 7.28 sends its clock in MILLISECONDS (measured: +10..15 per 10..15 ms frame), the harness in seconds.
			// Read as seconds, every Arena frame hit the 0.1 s clamp: all motion ran 6-12x fast and with the frame rate.
			// The unit is voted from the host step vs the wall clock (~1000 = ms, ~1 = s); the harness's fast frames vote neither.
			const double dh = hostTime - lastHostTime;
			if( dtClock > 0.002f && dh > 0.0 )
			{
				const double r = dh / dtClock;
				msVotes        = std::clamp( msVotes + ( r > 200.0 && r < 5000.0 ? 1 : r > 0.2 && r < 5.0 ? -1 : 0 ), -8, 8 );
			}
			hostScale = msVotes > 0 ? 0.001 : 1.0;
			dt        = float( dh * hostScale );
		}
		WatchTrigger( dtClock );
		lastHostTime  = hostTime;
		hostTimeValid = hostTimeSeen;
		dt            = std::min( 0.1f, std::max( 0.0f, dt ) );

		UpdateModel();
		UpdateEyes();
		UpdateAudio( dt );

		FrameState fs;
		fs.width   = (int)currentViewport.width;
		fs.height  = (int)currentViewport.height;
		fs.hostFBO = gl->HostFBO;
		BuildFrame( fs, dt );

#if LOADEHR_FX
		if( gl->numInputTextures > 0 && gl->inputTextures[ 0 ] )
		{
			const FFGLTextureStruct& in = *gl->inputTextures[ 0 ];
			fs.inputTex                 = in.Handle;
			fs.inputW = (int)in.Width, fs.inputH = (int)in.Height;
			fs.inputHwW = (int)in.HardwareWidth, fs.inputHwH = (int)in.HardwareHeight;
		}
#endif
		const bool perfOn = On( P_DEBUG_LOG ); // W29: the PERF line
		renderer.perf.Begin( perfOn );
		// LoadEhr W1: every filled slot gets what its hops will need, ahead of use (the rest model: only its liquid SDF; the renderer
		// keeps its other caches itself): the incoming model's shards / dust / liquid SDF are ready before its hop begins
		const unsigned uses = ChainUses() | ( fs.sim.engine > 0 && fs.vol.active ? kUseSim : 0u );
		for( int i = 0; i < kModelSlots; ++i )
			if( slots[ i ].current && SlotFilled( i ) )
				renderer.PrepareSlot( i, slots[ i ].current.get(), fs, i == chain.cur ? uses & kUseLiquid : uses );
		// the pair: the hop's outgoing model (cur) and incoming one (next); nothing loaded = nothing drawn (transparent)
		const int cur = chain.cur, next = chain.next;
		renderer.Render( fs, cur >= 0 ? slots[ cur ].current.get() : nullptr, next >= 0 ? slots[ next ].current.get() : nullptr, std::max( cur, 0 ),
		                 next >= 0 ? next : ( std::max( cur, 0 ) + 1 ) % kModelSlots );
		renderer.perf.End();
		if( perfOn )
			PerfTick( now, dtClock, fs );
		else
			perfIv.clear(), perfCpu.clear();
#if LOADEHR_FX
		// hand a finished readback to Vision (it runs on its own queue; nothing here waits for it)
		std::vector< uint8_t > px;
		int fw = 0, fh = 0;
		if( renderer.TakeFaceFrame( px, fw, fh ) && On( P_FACE_TRACK ) )
			faceTracker.Submit( std::move( px ), fw, fh );
#endif
		return FF_SUCCESS;
	}

	// ---- W29: one PERF line per second while Debug Log is on. fps = 1 / the interval between this instance's
	// ProcessOpenGL calls (what the host delivers: median, and the slow end = 1 / the p95 interval); cpu = wall ms inside
	// ProcessOpenGL (median, p95); render = Render's wall ms and its passes' (PerfProbe; GPU included only offline with
	// LOADEHR_PERF_FINISH=1).
	void PerfTick( std::chrono::steady_clock::time_point start, float interval, const FrameState& fs )
	{
		const auto now = std::chrono::steady_clock::now();
		if( perfCpu.empty() )
			perfStart = now;
		if( interval > 0.0f && interval < 0.3f )
			perfIv.push_back( interval * 1000.0f );
		perfCpu.push_back( std::chrono::duration< float, std::milli >( now - start ).count() );
		if( std::chrono::duration< float >( now - perfStart ).count() < 1.0f || perfIv.size() < 2 )
			return;
		auto q = []( std::vector< float > v, float f ) {
			std::sort( v.begin(), v.end() );
			return v[ std::min( v.size() - 1, size_t( f * v.size() ) ) ];
		};
		const float ivMed = q( perfIv, 0.5f ), ivP95 = q( perfIv, 0.95f );
		PerfProbe& p = renderer.perf;
		char passes[ 512 ];
		int n = 0;
		for( int i = 0; i < PerfProbe::Count && n < (int)sizeof( passes ) - 40; ++i )
		{
			const double c = p.frames ? p.ms[ i ] / p.frames : 0.0;
			if( c >= 0.05 )
				n += snprintf( passes + n, sizeof( passes ) - n, " %s %.2f", PerfProbe::Name( i ), c );
		}
		passes[ n ] = 0;
		const std::string& f = chain.cur >= 0 ? slots[ chain.cur ].file : modelParam[ 0 ];
		const size_t sl      = f.find_last_of( '/' );
		Log( "PERF %p %dx%d '%s' n %zu fps %.1f p95 %.1f | iv ms %.2f/%.2f | cpu ms %.2f p95 %.2f | render ms %.2f:%s", (void*)this, fs.width, fs.height,
		     ( sl == std::string::npos ? f : f.substr( sl + 1 ) ).c_str(), perfIv.size(), 1000.0f / ivMed, 1000.0f / ivP95, ivMed, ivP95,
		     q( perfCpu, 0.5f ), q( perfCpu, 0.95f ), p.frames ? p.total / p.frames : 0.0, passes );
		p.Take();
		perfIv.clear(), perfCpu.clear();
		perfStart = now;
		// LoadEhr W1: GPU memory per model slot (the model: textures with mips, geometry, LOD index buffers; + the slot's caches:
		// shards, dust, liquid SDF, sim obstacle SDF). Models shared by several slots / instances (the GPU cache) count once each here.
		char mem[ 512 ];
		int k = 0;
		double total = 0;
		for( int i = 0; i < kModelSlots; ++i )
		{
			const GpuModel* m = slots[ i ].current.get();
			double lod        = 0;
			if( m )
				for( int l = 1; l < kLodLevels; ++l )
					lod += double( m->lods[ l ].triangles ) * 3 * 4;
			const double mb = m ? ( double( m->texBytes ) + double( m->geometryBytes ) + lod ) / 1048576.0 : 0.0;
			const double cb = double( renderer.SlotBytes( i ) ) / 1048576.0;
			total += mb + cb;
			k += snprintf( mem + k, sizeof( mem ) - size_t( k ), " %c %s %.1f MB + caches %.1f MB |", "ABCD"[ i ], m ? FileName( m->path ).c_str() : "-", mb, cb );
			if( k >= (int)sizeof( mem ) - 80 )
				break;
		}
		Log( "MEM %p slots:%s total %.1f MB; chain %c -> %c t %.2f", (void*)this, mem, total, chain.cur >= 0 ? "ABCD"[ chain.cur ] : '-',
		     chain.next >= 0 ? "ABCD"[ chain.next ] : '-', chain.t );
	}

	// ---- W28: what does a clip (re)trigger look like from here? A new instance (most triggers in Arena 7.28: InitGL with a new
	// address), the host clock jumping back, or the plugin not being drawn for a while (the clip was off, now on again). Each is
	// logged (TRIG ...) and restarts what plays on a trigger (Transition Play, the lava's spread). Param bursts are logged only.
	void WatchTrigger( float dtClock )
	{
		const char* why = nullptr;
		char buf[ 160 ];
		if( frames == 0 )
			snprintf( buf, sizeof( buf ), "first frame (host time %.3f, bar phase %.3f, bpm %.1f)", hostTime, barPhase, bpm ), why = buf;
		else if( dtClock > 0.3f )
			snprintf( buf, sizeof( buf ), "not drawn for %.2f s (host time %.3f)", dtClock, hostTime ), why = buf;
		else if( hostTimeValid && ( hostTime - lastHostTime ) * hostScale < -0.05 )
			snprintf( buf, sizeof( buf ), "host time back %.3f -> %.3f", lastHostTime, hostTime ), why = buf;
		if( setCalls >= 40 )
			Log( "TRIG instance %p frame %llu: %u param sets since the last frame (%u changed), host time %.3f", (void*)this,
			     (unsigned long long)frames, setCalls, setChanged, hostTime );
		setCalls = setChanged = 0;
		if( hostTimeValid && std::fabs( ( hostTime - lastHostTime ) * hostScale - dtClock ) > 0.5 && !why && frames >= lastJumpLog + 600 )
			lastJumpLog = frames, Log( "TRIG instance %p frame %llu: host time jumped %.3f -> %.3f over %.3f s", (void*)this, (unsigned long long)frames, lastHostTime, hostTime, dtClock );
		if( why )
		{
			Log( "TRIG instance %p frame %llu: %s -> restart", (void*)this, (unsigned long long)frames, why );
			OnTrigger();
		}
		++frames;
	}
	void OnTrigger()
	{
		transRestart = true;
		videoRestart = true; // W2: the projector's video starts from its first frame
		crackAuto    = true; // the lava spreads from nothing, then stays (W28)
	}

	FFResult SetTime( double time ) override
	{
		hostTime     = time;
		hostTimeSeen = true;
		return FF_SUCCESS;
	}

	// Harness only (LoadEhrTestAdvance): run every clock forward as if `seconds` of show time had passed at
	// the current parameter values, without rendering (10 fps steps, the longest frame ProcessOpenGL accepts).
	// fromZero: every integrated phase starts from 0 first, so a test knows exactly when each one wraps.
	void FastForward( double seconds, bool fromZero )
	{
		if( fromZero )
		{
			clk = Clocks();
			// W25: the beat envelopes too (a puff's decay carried over from the load window made Exhale On Beat vary run to run)
			beatCount = lastPuffBeat = lastRingBeat = lastBurstBeat = 0;
			puffT = burstT = 10.0f;
		}
		FrameState fs;
		fs.width = fs.height = 1080;
		for( ; seconds > 1e-9; seconds -= 0.1 )
			BuildFrame( fs, float( std::min( 0.1, seconds ) ) );
	}

private:
	// LoadEhr: a model comes ONLY from the file a slot's FILE param points at (no folder scan, no list, no default path)
	void SetModelFile( int slot, const std::string& v )
	{
		if( v == modelParam[ slot ] )
			return;
		Log( "Model %c <- '%s'", "ABCD"[ slot ], v.c_str() );
		modelParam[ slot ] = v;
	}

	static const char* TextDefault( unsigned id ) { return id == P_TEXT ? "LOADEHR" : id == P_FONT_NAME ? "Helvetica Neue" : ""; }
	static const std::vector< std::string >& Families()
	{
		static const std::vector< std::string > fam = FontFamilies(); // once per process
		return fam;
	}
	// Font Name is the truth; the Font list follows it and Font Style lists its faces
	void SetFontName( const std::string& v )
	{
		if( v == texts[ P_FONT_NAME ] && !styles.empty() )
			return;
		texts[ P_FONT_NAME ] = v;
		const auto& fam      = Families();
		const auto it        = std::find( fam.begin(), fam.end(), v );
		const float idx      = it == fam.end() ? 0.0f : float( it - fam.begin() + 1 );
		if( values[ P_FONT ] != idx )
		{
			values[ P_FONT ] = idx;
			RaiseParamEvent( P_FONT, FF_EVENT_FLAG_VALUE );
		}
		styles = FontStyles( v );
		std::vector< std::string > names = { "Default" };
		std::vector< float > vals        = { 0.0f };
		for( size_t i = 0; i < styles.size(); ++i )
			names.push_back( styles[ i ] ), vals.push_back( float( i + 1 ) );
		SetParamElements( P_FONT_STYLE, names, vals, true );
	}
	void SetStatus( int slot, const std::string& s )
	{
		if( s == status[ slot ] )
			return;
		status[ slot ] = s;
		Log( "STATUS %c %s", "ABCD"[ slot ], s.c_str() );
		RaiseParamEvent( SlotParam( slot, SF_STATUS ), FF_EVENT_FLAG_VALUE );
	}
	static std::string FileName( const std::string& path ) { return path.substr( path.find_last_of( '/' ) + 1 ); }
	// the Status lines: per slot what it holds; with nothing loaded anywhere, Status A says the output is transparent
	void UpdateStatuses()
	{
		bool any = false;
		for( const ModelSlot& m : slots )
			any = any || m.current || m.pending || m.ticket;
		for( int i = 0; i < kModelSlots; ++i )
		{
			const ModelSlot& m = slots[ i ];
			if( IsGenSpec( m.file ) )
				continue; // text / logo: the generator's own status (set on load)
			std::string st;
			if( m.file.empty() )
				st = i == 0 && !any ? "Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file" : "Empty: skipped by the chain";
			else if( m.failed )
				st = "Load failed: " + m.error;
			else if( m.ticket || m.pending )
				st = "Loading " + FileName( m.file ) + "...";
			else if( m.current )
				st = FileName( m.file ) + Fmt( ": %zu tris, %zu parts", m.current->triangleCount, m.current->parts.size() );
			SetStatus( i, st );
		}
	}
	// the generated model's spec: every value that changes the mesh (the Letters dials animate it, they are not in here)
	std::string TextLogoSpec()
	{
		std::map< std::string, std::string > f;
		auto num = [ & ]( const char* k, unsigned id ) {
			char b[ 32 ];
			snprintf( b, sizeof( b ), "%.4g", values[ id ] );
			f[ k ] = b;
		};
		const bool text = Opt( P_MODEL_SOURCE ) == 1;
		f[ "source" ]   = text ? "text" : "logo";
		if( text )
		{
			f[ "text" ] = texts[ P_TEXT ], f[ "font" ] = texts[ P_FONT_NAME ], f[ "fontfile" ] = texts[ P_FONT_FILE ];
			const int st = Opt( P_FONT_STYLE );
			f[ "style" ] = st > 0 && st <= (int)styles.size() ? styles[ st - 1 ] : "";
			num( "align", P_ALIGN ), num( "linespacing", P_LINE_SPACING ), num( "letterspacing", P_LETTER_SPACING );
		}
		else
		{
			f[ "file" ] = texts[ P_LOGO_FILE ];
			num( "colours", P_LOGO_COLOURS ), num( "threshold", P_TRACE_THRESHOLD ), num( "smooth", P_TRACE_SMOOTH ), num( "invert", P_TRACE_INVERT );
		}
		num( "detail", P_CURVE_DETAIL ), num( "depth", P_EXTRUDE_DEPTH ), num( "bevel", P_BEVEL_SIZE ), num( "segments", P_BEVEL_SEGMENTS );
		num( "bevelshape", P_BEVEL_SHAPE ), num( "backcap", P_BACK_CAP ), num( "outlineonly", P_OUTLINE_ONLY ), num( "wall", P_WALL );
		// W2 Shatter Mode Pieces: the fractured copy is built with the model (cached by the spec: a new Shard Count rebuilds it)
		if( Opt( P_SHATTER_MODE ) > 0 )
			f[ "frac" ] = std::to_string( std::clamp( (int)std::lround( values[ P_SHARD_COUNT ] ), 8, 512 ) );
		return MakeSpec( f );
	}

	int Opt( unsigned id ) const { return (int)std::lround( values[ id ] ); }
	// a colour picker quartet (HUE id first) -> rgb; its ALPHA is values[ hueId + 3 ]
	void Colour( unsigned hueId, float* rgb ) const { HsbToRgb( values[ hueId ], values[ hueId + 1 ], values[ hueId + 2 ], rgb ); }
	float Alpha( unsigned hueId ) const { return values[ hueId + 3 ]; }
	// the picked colour with its HUE advanced by `turns` in HSB space (saturation and brightness kept)
	void ColourDrift( unsigned hueId, float turns, float* rgb ) const { HsbToRgb( values[ hueId ] + turns, values[ hueId + 1 ], values[ hueId + 2 ], rgb ); }
	bool On( unsigned id ) const { return values[ id ] > 0.5f; }

	// ---- model selection, background load, incremental upload, atomic swap. LoadEhr W1: four slots (A..D), each its FILE param;
	// slot A shows the text / logo spec instead when Model Source says so. Every filled slot loads and uploads in the background
	// whatever the chain shows (the old model keeps drawing until the new one swaps in).
	struct ModelSlot
	{
		std::string file, requested, gpuKey;
		int requestedMaxTex = 0;
		std::shared_ptr< LoadTicket > ticket;
		std::shared_ptr< GpuModel > pending, current;
		std::chrono::steady_clock::time_point switchStart;
		bool failed = false; // the last load of `file` failed (the slot is skipped by the chain unless an older model still draws)
		std::string error;
		double swapMs = 0; // request -> on the GPU
	};

	void UpdateModel()
	{
		// slot A shows Model A, or the text / logo spec. A new spec starts building at once when slot A is idle; while a build runs,
		// edits coalesce and the latest spec starts when it lands (typing never queues a backlog)
		const std::string want = Opt( P_MODEL_SOURCE ) == 0 ? modelParam[ 0 ] : TextLogoSpec();
		if( want != slots[ 0 ].file && ( !IsGenSpec( want ) || !IsGenSpec( slots[ 0 ].file ) || ( !slots[ 0 ].ticket && !slots[ 0 ].pending ) ) )
		{
			if( IsGenSpec( want ) && LogVerbose() )
				Log( "GEN request %s", ShortSpecName( want ).c_str() );
			slots[ 0 ].file = want;
		}
		for( int i = 1; i < kModelSlots; ++i )
			slots[ i ].file = modelParam[ i ];
		for( int i = 0; i < kModelSlots; ++i )
		{
			UpdateSlot( slots[ i ], reloadRequested[ i ], i );
			reloadRequested[ i ] = false;
		}
		UpdateStatuses();
	}

	void UpdateSlot( ModelSlot& m, bool fresh, int slot )
	{
		const std::string& wanted = m.file;
		static const int kMaxTex[] = { 0, 4096, 2048 };
		const int maxTex           = kMaxTex[ std::clamp( Opt( P_MAX_TEX ), 0, 2 ) ];
		if( wanted != m.requested || ( fresh && !wanted.empty() ) || ( maxTex != m.requestedMaxTex && !wanted.empty() ) )
		{
			m.requested       = wanted;
			m.requestedMaxTex = maxTex;
			m.failed          = false;
			AbandonTicket( m );
			m.pending.reset();
			if( wanted.empty() )
			{
				m.current.reset();
				return;
			}
			if( fresh )
				ForgetGpuModelsForPath( wanted );
			std::string key = ModelKey( wanted, maxTex );
			m.gpuKey        = key + "#" + ShareGroupKey();
			if( !fresh && !key.empty() )
				m.pending = FindGpuModel( m.gpuKey );
			if( !m.pending )
				m.ticket = RequestLoad( wanted, maxTex, fresh );
			m.switchStart = std::chrono::steady_clock::now();
		}

		if( m.ticket && m.ticket->done.load() )
		{
			if( m.ticket->model )
			{
				m.pending = FindGpuModel( m.gpuKey );
				if( !m.pending )
				{
					m.pending = std::make_shared< GpuModel >( m.ticket->model );
					RegisterGpuModel( m.gpuKey, m.pending );
				}
			}
			if( !m.ticket->model )
			{
				Log( "Load failed (slot %c), keeping the previous model: %s", "ABCD"[ slot ], m.ticket->error.c_str() );
				m.failed = true;
				m.error  = m.ticket->error;
				if( IsGenSpec( m.ticket->path ) )
					SetStatus( slot, m.ticket->status );
				FFGLLog::LogToHost( ( "LoadEhr: could not load " + LogName( m.ticket->path ) + ": " + m.ticket->error ).c_str() );
			}
			m.ticket.reset();
		}

		if( m.pending )
		{
			if( m.pending->UploadStep( 2 ) )
			{
				m.current = m.pending;
				m.pending.reset();
				m.swapMs = std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - m.switchStart ).count();
				Log( "SWAP to %s (slot %c, %zu tris, %zu parts) %.0f ms after request", LogName( m.current->path ).c_str(), "ABCD"[ slot ], m.current->triangleCount,
				     m.current->parts.size(), m.swapMs );
				if( IsGenSpec( m.current->path ) )
					SetStatus( slot, m.current->status );
			}
		}
	}

	static void AbandonTicket( ModelSlot& m )
	{
		// the worker holds one reference; if nobody else shares the job, stop it
		if( m.ticket && m.ticket.use_count() <= 2 )
			m.ticket->cancel.store( true );
		m.ticket.reset();
	}

	// LoadEhr W1: the Parts group is labelled after the DOMINANT model of the chain (the one the picture is nearer to): it is
	// relabelled at a hop's midpoint. The values stay per part SLOT (Part 2 Visible hides part slot 2 of every model), as MaskEhr's
	// Model B always did.
	void ApplyPartNames( const GpuModel* current )
	{
		partNamesFor = current ? current->serial : 0;
		size_t n     = current ? current->slotNames.size() : 0;
		for( unsigned s = 0; s < kMaxParts; ++s )
		{
			bool used = s < n;
			std::string base, more;
			if( used )
			{
				base = current->slotNames[ s ];
				const size_t shared = std::count_if( current->parts.begin(), current->parts.end(), [ s ]( const GpuPart& p ) { return p.slot == (int)s; } );
				if( s == kMaxParts - 1 && shared > 1 )
					more = " +" + std::to_string( shared - 1 );
			}
			const char* fields[ kPartFields ] = { "Visible", "Hue Shift", "Glow", "Skin", "Surface" };
			for( unsigned f = 0; f < kPartFields; ++f )
			{
				unsigned id = PartParam( s, PartField( f ) );
				bool show   = used && ( f != PF_SKIN || LOADEHR_FX );
				SetParamVisibility( id, show, true );
				SetParamDisplayName( id, used ? CapName( base, more, fields[ f ] ) : "Part " + std::to_string( s + 1 ) + " " + fields[ f ], true );
			}
		}
	}

	// ---- Light C eye placement. Place At Eyes / Fit Cone / the first Enable per model start an eye search on
	// the current model (worker thread, cached per geometry); the answer is written into the C params through
	// value-change events, so it is visible, editable and saved with the composition.
	enum EyeRequest { EyeNone, EyePlace, EyeFitCone, EyeAutoPlace };

	// positions moved by hand before any placement: the auto-place leaves them alone (the buttons still work)
	bool EyesSetByHand() const
	{
		if( !eyePlacedFor.empty() )
			return false;
		for( unsigned id = P_LC_L_POS_X; id <= P_LC_R_POS_Z; ++id )
			if( std::fabs( values[ id ] - ParamDefs()[ id ].def ) > 1e-4f )
				return true;
		return false;
	}

	// the eye lights, smoke emitted from the eyes (W20) or eye-to-eye bolts need the eyes
	bool WantEyes() const
	{
		return On( P_LC_ENABLE ) || ( Opt( P_SMOKE_ENGINE ) > 0 && values[ P_EMIT_EYES ] > 0.0f ) ||
		       ( Opt( P_BOLT_PATH ) == 1 && ( values[ P_STRIKE_RATE ] > 0.0f || Opt( P_STRIKE_BEAT ) > 0 || strikeCount > 0 ) );
	}

	// LoadEhr W1: Place At Eyes / Fit Cone / the auto-place work on the chain's ANCHOR model (the first filled slot: Model A unless
	// it is empty) and write the C params, as MaskEhr did for Model File. Every other filled slot gets its own eye search (cached
	// per geometry, ahead of use); while the chain shows another model the openings move by the difference between its eyes and
	// the anchor's (EyeOpen), gliding across the middle of each hop.
	void UpdateEyes()
	{
		const int as = chain.n > 0 ? chain.filled[ 0 ] : 0;
		UpdateEyePlacement( as );
		UpdateChainEyes( as );
	}

	struct EyeSlot
	{
		std::shared_ptr< EyeJob > job;
		std::string key;
		bool found = false;
		float pos[ 2 ][ 3 ] = {}; // left, right opening, mask space
	};
	EyeSlot eyeSlot[ kModelSlots ];
	float eyeShift[ 2 ][ 3 ] = {}; // this frame: left / right opening offset from the anchor's (mask space)

	// the eye query for a model: what Auto Fit sizes it by, glass left out (lenses are openings to look through)
	EyeQuery EyeQueryFor( const GpuModel& m ) const
	{
		EyeQuery query;
		float target[ 4 ];
		TargetFit( m, target, &query.include );
		for( size_t i = 0; i < m.parts.size(); ++i )
			if( m.parts[ i ].glass > 0.0f )
				query.include[ i ] = 0;
		for( int k = 0; k < 3; ++k )
			query.fitCenter[ k ] = target[ k ];
		query.fitScale = target[ 3 ];
		return query;
	}

	void UpdateChainEyes( int anchorSlot )
	{
		std::memset( eyeShift, 0, sizeof( eyeShift ) );
		if( chain.n < 2 || !WantEyes() )
			return;
		for( int k = 0; k < chain.n; ++k )
		{
			const int i              = chain.filled[ k ];
			const GpuModel* m        = slots[ i ].current.get();
			EyeSlot& e               = eyeSlot[ i ];
			if( !m || !m->geometry || IsGenSpec( m->path ) )
			{
				e.found = false;
				continue;
			}
			EyeQuery q            = EyeQueryFor( *m );
			const std::string key = std::to_string( m->serial ) + " " + q.Key();
			if( !e.job || e.key != key )
			{
				e.job   = RequestEyes( m->geometry, q, "" );
				e.key   = key;
				e.found = false;
				continue;
			}
			if( !e.job->done.load() || e.found )
				continue;
			const EyeResult& r = e.job->result;
			if( !r.found )
				continue;
			const bool fit = On( P_AUTO_FIT );
			const float sc = fit ? 1.0f : 1.0f / q.fitScale;
			for( int c = 0; c < 3; ++c )
			{
				e.pos[ 0 ][ c ] = fit ? r.left.center[ c ] : r.left.center[ c ] * sc + q.fitCenter[ c ];
				e.pos[ 1 ][ c ] = fit ? r.right.center[ c ] : r.right.center[ c ] * sc + q.fitCenter[ c ];
			}
			e.found = true;
			Log( "EYES slot %c: [%s] left (%.3f %.3f %.3f) right (%.3f %.3f %.3f) (the chain moves the C openings by its offset from slot %c's)", "ABCD"[ i ],
			     r.kind.c_str(), e.pos[ 0 ][ 0 ], e.pos[ 0 ][ 1 ], e.pos[ 0 ][ 2 ], e.pos[ 1 ][ 0 ], e.pos[ 1 ][ 1 ], e.pos[ 1 ][ 2 ], "ABCD"[ anchorSlot ] );
		}
		const EyeSlot& A = eyeSlot[ anchorSlot ];
		if( !A.found || chain.cur < 0 )
			return;
		const float w = chain.next >= 0 ? SmoothStep( 0.35f, 0.65f, chain.t ) : 0.0f; // the glide across the hop's middle
		for( int side = 0; side < 2; ++side )
			for( int c = 0; c < 3; ++c )
			{
				const EyeSlot& E0 = eyeSlot[ chain.cur ];
				const float o0    = E0.found ? E0.pos[ side ][ c ] - A.pos[ side ][ c ] : 0.0f;
				float o1          = o0;
				if( chain.next >= 0 )
				{
					const EyeSlot& E1 = eyeSlot[ chain.next ];
					o1                = E1.found ? E1.pos[ side ][ c ] - A.pos[ side ][ c ] : 0.0f;
				}
				eyeShift[ side ][ c ] = o0 + ( o1 - o0 ) * w;
			}
	}

	// an eye opening in mask space (Light C's positions + the chain's eye offset). side 0 = left, 1 = right (Mirror X mirrors left)
	void EyeOpen( int side, float* open ) const
	{
		const bool mirror = On( P_LC_MIRROR );
		const int set     = side == 1 && !mirror ? 1 : 0;
		for( int k = 0; k < 3; ++k )
			open[ k ] = values[ ( set ? P_LC_R_POS_X : P_LC_L_POS_X ) + k ] + eyeShift[ set ][ k ];
		if( side == 1 && mirror )
			open[ 0 ] = -open[ 0 ];
	}

	void UpdateEyePlacement( int anchorSlot )
	{
		const std::shared_ptr< GpuModel >& current = slots[ anchorSlot ].current;
		const std::string& modelFile = slots[ anchorSlot ].file;
		const bool modelReady = current && current->geometry && current->path == modelFile && !IsGenSpec( modelFile ); // text / logos have no eyes
		// the first switch-on per model places them
		const bool wantEyes = WantEyes();
		if( eyeRequest == EyeNone && wantEyes && modelReady && eyePlacedFor != modelFile && !EyesSetByHand() )
			eyeRequest = EyeAutoPlace; // first switch-on for this model: give eye beams right away
		if( eyeRequest == EyeNone || !modelReady )
			return;
		// the finder sees what Auto Fit sizes by (Fit To: Visible Parts = hidden stands / props are left out), in that fit
		const EyeQuery query = EyeQueryFor( *current );
		const std::string key = std::to_string( current->serial ) + " " + query.Key();
		if( !eyeJob || eyeJobKey != key )
		{
			std::string dbg = getenv( "LOADEHR_EYE_DEBUG" ) ? getenv( "LOADEHR_EYE_DEBUG" ) : "";
			if( !dbg.empty() && dbg.back() == '/' ) // a folder: one image per model, named after its folder + file
			{
				std::string name = modelFile.substr( modelFile.find_last_of( '/', modelFile.find_last_of( '/' ) - 1 ) + 1 );
				std::replace_if( name.begin(), name.end(), []( char c ) { return c == '/' || c == ' '; }, '_' );
				dbg += name + ".png";
			}
			eyeJob       = RequestEyes( current->geometry, query, dbg );
			eyeJobKey    = key;
			eyeJobLogged = false;
		}
		if( !eyeJob->done.load() )
			return;
		const EyeResult& r = eyeJob->result;
		if( !eyeJobLogged )
		{
			Log( "EYES %s: [%s] %s (%.0f ms, off the render thread)", modelFile.c_str(), r.found ? r.kind.c_str() : "none", r.reason.c_str(), r.ms );
			for( const std::string& line : r.log )
				Log( "EYES   %s", line.c_str() );
			eyeJobLogged = true;
		}
		const EyeRequest req = eyeRequest;
		eyeRequest           = EyeNone;
		if( req == EyeAutoPlace || req == EyePlace )
		{
			eyePlacedFor = modelFile; // also when nothing was found: the auto-place runs once per model
			RaiseParamEvent( P_LC_PLACED, FF_EVENT_FLAG_VALUE );
		}
		if( !r.found )
		{
			Log( "EYES %s: params left untouched", req == EyeFitCone ? "Fit Cone" : "Place At Eyes" );
			return;
		}
		// the finder works in fitted mask space; without Auto Fit, mask space is the file's own units
		const bool fit  = On( P_AUTO_FIT );
		const float s   = fit ? 1.0f : 1.0f / query.fitScale;
		auto toMask     = [ & ]( const float* c, int k ) { return fit ? c[ k ] : c[ k ] * s + query.fitCenter[ k ]; };
		auto write      = [ & ]( unsigned id, float v ) {
			const ParamDef& d = ParamDefs()[ id ];
			values[ id ]      = std::min( d.max, std::max( d.min, v ) );
			RaiseParamEvent( id, FF_EVENT_FLAG_VALUE );
		};
		const float radius = std::max( r.left.radius, r.right.radius ) * s;
		// a cavity eye (closed behind): the lights sit in front of the back wall, just behind the rim, so the wall does
		// not block the beam and Front Only still starts the haze at the opening
		const float wall = std::min( r.left.depth > 0 ? r.left.depth : 1e9f, r.right.depth > 0 ? r.right.depth : 1e9f ) * s;
		if( req != EyeFitCone && wall < 1e8f && values[ P_LC_BEHIND ] > 0.6f * wall )
		{
			write( P_LC_BEHIND, 0.6f * wall );
			Log( "EYES placement: in the cavity, %.3f behind the rim (the back wall is %.3f behind it)", values[ P_LC_BEHIND ], wall );
		}
		else if( req != EyeFitCone )
			Log( "EYES placement: %s, Behind Depth %.3f behind the opening", wall < 1e8f ? "in the cavity" : "behind the see-through opening", values[ P_LC_BEHIND ] );
		const float behind = values[ P_LC_BEHIND ];
		// full cone that just fills the opening from Behind Depth, plus a margin
		// capped at 40 deg: a light close behind a shallow opening would otherwise flood the whole frame
		write( P_LC_CONE, std::min( 40.0f, 2.0f * std::atan( radius / std::max( behind, 1e-3f ) ) / kDeg * 1.1f + 2.0f ) );
		if( req != EyeFitCone )
		{
			for( int k = 0; k < 3; ++k )
			{
				write( P_LC_L_POS_X + k, toMask( r.left.center, k ) );
				write( P_LC_R_POS_X + k, toMask( r.right.center, k ) );
			}
			write( P_LC_AIM_MODE, 0 ); // Through Hole
			write( P_LC_ANCHOR, 1 );
			// Mirror X mirrors about x = 0 (the bbox centre); eyes off that axis get their own right position
			const float offset = 0.5f * ( r.left.center[ 0 ] + r.right.center[ 0 ] );
			if( On( P_LC_MIRROR ) && std::fabs( offset ) > 0.1f * std::max( r.left.radius, r.right.radius ) )
			{
				write( P_LC_MIRROR, 0 );
				Log( "EYES eyes centred at x %.3f, not 0 (hole radius %.3f): Mirror X off so each light sits in its own hole", offset * s, radius );
			}
		}
		Log( "EYES %s: cone %.1f deg at Behind Depth %.2f%s", req == EyeFitCone ? "Fit Cone" : "placed", values[ P_LC_CONE ], behind,
		     req == EyeFitCone ? "" : Fmt( ", left (%.3f %.3f %.3f) right (%.3f %.3f %.3f)", values[ P_LC_L_POS_X ], values[ P_LC_L_POS_Y ], values[ P_LC_L_POS_Z ],
		                                   values[ P_LC_R_POS_X ], values[ P_LC_R_POS_Y ], values[ P_LC_R_POS_Z ] ).c_str() );
	}

	// ---- audio: FFT buffer -> smoothed low / mid / high bands
	void UpdateAudio( float dt )
	{
		const ParamInfo* info = FindParamInfo( P_FFT );
		float bands[ 3 ]      = { 0, 0, 0 };
		if( info && !info->elements.empty() )
		{
			const size_t n    = info->elements.size();
			const size_t e1   = std::max< size_t >( 1, n / 16 ), e2 = std::max< size_t >( e1 + 1, n / 4 );
			const size_t lo[] = { 0, e1, e2 }, hi[] = { e1, e2, n };
			for( int b = 0; b < 3; ++b )
			{
				float s = 0;
				for( size_t i = lo[ b ]; i < hi[ b ]; ++i )
					s += info->elements[ i ].value;
				bands[ b ] = s / float( std::max< size_t >( 1, hi[ b ] - lo[ b ] ) );
			}
		}
		const float gain = values[ P_AUDIO_GAIN ];
		for( int b = 0; b < 3; ++b )
		{
			float target = std::min( 4.0f, bands[ b ] * gain * ( b == 0 ? 2.0f : b == 1 ? 4.0f : 8.0f ) );
			float rate   = target > audio[ b ] ? 30.0f : 6.0f; // fast attack, smooth release
			audio[ b ] += ( target - audio[ b ] ) * std::min( 1.0f, rate * dt );
		}
	}

	// ---- motion + parameters -> FrameState
	void BuildFrame( FrameState& fs, float dt )
	{
		EvalChain( dt ); // LoadEhr W1: which pair this frame shows (chain.cur -> chain.next at chain.t)
		for( int k = 0; k < 3; ++k )
			clk.spin[ k ] = std::fmod( clk.spin[ k ] + values[ P_SPIN_X + k ] * dt, 360.0 );
		clk.wobble = Wrap( clk.wobble + dt * values[ P_WOBBLE_SPEED ] * 2.0, 5.0 * kSmoothPeriod ); // read at x1 and x0.8

		const float beatPhase = barPhase * 4.0f - std::floor( barPhase * 4.0f );
		const float pulse     = BeatPulse( beatPhase );
		beatClock += dt;
		if( beatPhase < lastBeatPhase - 0.5f )
		{
			++beatCount;
			if( beatClock > 0.05f && beatClock < 4.0f )
				beatPeriod = beatClock;
			beatClock = 0.0f;
		}
		lastBeatPhase = beatPhase;
		const float wob       = values[ P_WOBBLE_AMOUNT ];

		UpdateGlide( dt );
		float rot[ 3 ];
		for( int k = 0; k < 3; ++k )
			rot[ k ] = PV( P_ROT_X + k ) + float( clk.spin[ k ] ) + wob * 18.0f * Smooth1D( clk.wobble, float( k ) * 3.3f );
		rot[ 0 ] -= values[ P_BEAT_NOD ] * 18.0f * pulse;
		float pos[ 3 ];
		for( int k = 0; k < 3; ++k )
			pos[ k ] = PV( P_POS_X + k ) + wob * 0.08f * Smooth1D( clk.wobble * 0.8, 10.0f + k * 2.1f );
		float scale = values[ P_SCALE ] * ( 1.0f + values[ P_BEAT_BOUNCE ] * 0.3f * pulse ) * ( 1.0f + values[ P_AUDIO_SCALE ] * 0.5f * audio[ 0 ] );

		Mat4 M = Mat4::Translate( pos[ 0 ], pos[ 1 ], pos[ 2 ] ) * Mat4::RotZ( rot[ 2 ] * kDeg ) * Mat4::RotY( rot[ 1 ] * kDeg ) * Mat4::RotX( rot[ 0 ] * kDeg ) * Mat4::Scale( scale );
		const Mat4 anchor = M; // "mask space": the mask's pose and scale, independent of the file's units
		// LoadEhr W1: every loaded slot has its own fit (Fit To, gliding 0.5 s); the hop's outgoing model is drawn with fs.model,
		// the incoming one with fs.modelB: the same pose, each fitted by its own parts
		Mat4 MS[ kModelSlots ];
		float scaleS[ kModelSlots ];
		for( int i = 0; i < kModelSlots; ++i )
		{
			MS[ i ] = M, scaleS[ i ] = scale;
			const GpuModel* m = slots[ i ].current.get();
			if( !m )
				continue;
			UpdateFit( i, *m, dt );
			if( On( P_AUTO_FIT ) )
			{
				const float* f = fitNow[ i ];
				MS[ i ]        = M * Mat4::Scale( f[ 3 ] ) * Mat4::Translate( -f[ 0 ], -f[ 1 ], -f[ 2 ] );
				scaleS[ i ]    = scale * f[ 3 ];
			}
		}
		const int cur = std::max( chain.cur, 0 ), nxt = chain.next >= 0 ? chain.next : cur;
		M             = MS[ cur ];
		fs.modelScale = scaleS[ cur ];
		std::memcpy( fs.modelB, MS[ nxt ].m, sizeof( MS[ nxt ].m ) );
		fs.modelScaleB = scaleS[ nxt ];
		std::memcpy( fs.model, M.m, sizeof( M.m ) );
		// the dominant model (the nearer of the pair): what follows one model (sim obstacle, lightning, lava seeds, part names)
		domSlot = chain.cur;
		if( chain.next >= 0 && chain.t >= 0.5f && slots[ chain.next ].current )
			domSlot = chain.next;
		domM = MS[ std::max( domSlot, 0 ) ];
		for( int i = 0; i < kModelSlots; ++i )
			slotM[ i ] = MS[ i ];
		const GpuModel* dm = domSlot >= 0 ? slots[ domSlot ].current.get() : nullptr;
		if( ( dm ? dm->serial : 0 ) != partNamesFor )
			ApplyPartNames( dm );

		// camera: base (or a pose glide) + beat cut angle + shake, then vertigo (dolly zoom: distance and FOV
		// change together so the subject keeps its size)
		float camYaw = PV( P_ORBIT_YAW ), camPitch = PV( P_ORBIT_PITCH ), dist = PV( P_DISTANCE ), fov = PV( P_FOV ), roll = 0;
		UpdateCameraLive( camYaw, camPitch, dist, roll, dt );
		// Floor Camera: while the floor is on, look down onto it (with a level camera the floor sits right under
		// the mask and its reflection falls below the frame)
		const float floorK = values[ P_FLOOR_CAMERA ] * SmoothStep( 0.0f, 0.3f, std::max( values[ P_SHADOW_CATCHER ], values[ P_MIRROR ] ) );
		camPitch += 18.0f * floorK;
		dist *= 1.0f + 0.15f * floorK;
		const float vk = std::exp( values[ P_VERTIGO ] * 1.2f );
		dist *= vk;
		fov = std::min( 170.0f, std::max( 1.0f, 2.0f * std::atan( std::tan( fov * 0.5f * kDeg ) / vk ) / kDeg ) );
		const float yaw = camYaw * kDeg, pitch = std::max( -89.0f, std::min( 89.0f, camPitch ) ) * kDeg;
		const float at[ 3 ] = { 0, -0.35f * floorK * values[ P_SCALE ], 0 };
		float eye[ 3 ]      = { at[ 0 ] + dist * std::cos( pitch ) * std::sin( yaw ), at[ 1 ] + dist * std::sin( pitch ), at[ 2 ] + dist * std::cos( pitch ) * std::cos( yaw ) };
		float up[ 3 ]       = { 0, 1, 0 };
		if( roll != 0.0f )
		{
			// tilt the up vector about the view axis
			float f[ 3 ] = { -eye[ 0 ], -eye[ 1 ], -eye[ 2 ] }, r[ 3 ], u2[ 3 ];
			Mat4::Norm( f );
			Mat4::Cross( f, up, r );
			Mat4::Norm( r );
			Mat4::Cross( r, f, u2 );
			const float c = std::cos( roll * kDeg ), sn = std::sin( roll * kDeg );
			for( int k = 0; k < 3; ++k )
				up[ k ] = u2[ k ] * c + r[ k ] * sn;
		}
		Mat4 V              = Mat4::LookAt( eye, at, up );
		const float aspect  = float( std::max( 1, fs.width ) ) / float( std::max( 1, fs.height ) );
		Mat4 P              = Mat4::Perspective( fov * kDeg, aspect, std::max( 0.01f, dist * 0.01f ), dist + 200.0f );
		std::memcpy( fs.view, V.m, sizeof( V.m ) );
		std::memcpy( fs.proj, P.m, sizeof( P.m ) );
		std::memcpy( fs.camPos, eye, sizeof( eye ) );
		BuildCopies( fs, M, pos, scale, dt ); // after the camera: clone LODs use the distance to it
		// LoadEhr W1: the same copies round the incoming model's own fit (X M = copy  ->  X MB)
		fs.instB.clear();
		if( !fs.inst.empty() && chain.next >= 0 )
		{
			const Mat4 Minv = M.Inverse();
			for( const Instance& in : fs.inst )
			{
				Instance b = in;
				Mat4 im;
				std::memcpy( im.m, in.m, sizeof( im.m ) );
				const Mat4 bm = im * Minv * MS[ nxt ];
				std::memcpy( b.m, bm.m, sizeof( b.m ) );
				fs.instB.push_back( b );
			}
		}
#if LOADEHR_FX
		BuildFaces( fs, M, pos, V, P, dt );
#endif
		fs.transition      = chain.next >= 0 ? chain.t : 0.0f;
		fs.transitionStyle = chain.style;
		fs.transEase       = values[ P_TRANS_EASE ];
		fs.transStagger    = values[ P_TRANS_STAGGER ];
		fs.transSpread     = values[ P_TRANS_SPREAD ];
		// W26 Liquid: the wobble's ripples travel at 0.6 cycles / s x Liquid Speed (W28; phase wraps at 1: whole cycles in the shader)
		clk.liquid         = Wrap( clk.liquid + dt * 0.6 * values[ P_LIQUID_SPEED ], 1.0 );
		fs.liquid.goo      = values[ P_LIQUID_GOO ];
		fs.liquid.look     = Opt( P_LIQUID_LOOK );
		fs.liquid.wobble   = values[ P_LIQUID_WOBBLE ];
		fs.liquid.phase    = float( clk.liquid );
		fs.liquid.tintMix  = Alpha( P_LIQUID_COL_H );
		Colour( P_LIQUID_COL_H, fs.liquid.tint );

		Colour( P_TINT_H, fs.tint );
		Colour( P_RIM_H, fs.rimColor );
		fs.tintMix        = Alpha( P_TINT_H );
		fs.hue            = values[ P_HUE_SHIFT ];
		fs.saturation     = values[ P_SATURATION ];
		fs.brightness     = values[ P_BRIGHTNESS ];
		fs.metalOverride  = On( P_METAL_OVERRIDE );
		fs.metalValue     = values[ P_METAL_VALUE ];
		fs.roughScale     = values[ P_ROUGH_SCALE ];
		fs.normalStrength = values[ P_NORMAL_STRENGTH ];
		fs.normalFlipY    = On( P_NORMAL_FLIP_Y );
		fs.heightAmt      = values[ P_HEIGHT_DISPLACE ] * scale;
		fs.emissiveGlow   = values[ P_EMISSIVE_GLOW ];
		fs.rimGlow        = values[ P_RIM_GLOW ] * Alpha( P_RIM_H );
		const float audioGlow = values[ P_AUDIO_GLOW ] * 0.5f * audio[ 1 ];
		for( unsigned s = 0; s < kMaxParts; ++s )
		{
			fs.parts[ s ].visible   = On( PartParam( s, PF_VISIBLE ) );
			fs.parts[ s ].hue       = values[ PartParam( s, PF_HUE ) ];
			fs.parts[ s ].glow      = values[ PartParam( s, PF_GLOW ) ] + audioGlow;
			fs.parts[ s ].inputSkin = On( PartParam( s, PF_SKIN ) );
			fs.parts[ s ].surface   = values[ PartParam( s, PF_SURFACE ) ];
		}

		const float az = values[ P_KEY_AZIMUTH ] * kDeg, el = values[ P_KEY_ELEVATION ] * kDeg;
		fs.keyDir[ 0 ] = std::cos( el ) * std::sin( az ), fs.keyDir[ 1 ] = std::sin( el ), fs.keyDir[ 2 ] = std::cos( el ) * std::cos( az );
		const float faz = az + 150.0f * kDeg, fel = 12.0f * kDeg;
		fs.fillDir[ 0 ] = std::cos( fel ) * std::sin( faz ), fs.fillDir[ 1 ] = std::sin( fel ), fs.fillDir[ 2 ] = std::cos( fel ) * std::cos( faz );
		const float ki           = values[ P_KEY_INTENSITY ] * Alpha( P_KEY_COL_H );
		const float fillTint[ 3 ] = { 0.75f, 0.85f, 1.0f };
		float key[ 3 ];
		Colour( P_KEY_COL_H, key );
		for( int k = 0; k < 3; ++k )
		{
			fs.keyColor[ k ]  = key[ k ] * ki;
			fs.fillColor[ k ] = fillTint[ k ] * ki * 0.2f;
		}
		fs.ambient      = values[ P_AMBIENT ];
		fs.envIntensity = values[ P_ENV_INTENSITY ];
		fs.envSource    = Opt( P_ENV_SOURCE );
		if( !LOADEHR_FX && fs.envSource == 1 )
			fs.envSource = 0;
		fs.envRotate = values[ P_ENV_ROTATE ] * kDeg;
		fs.exposure  = values[ P_EXPOSURE ];
		fs.tonemap   = Opt( P_TONEMAP );

		BuildSurface( fs, pulse, dt );
		StepJelly( anchor, dt ); // W2: before BuildShape reads it
		BuildShape( fs, pulse, dt );
		BuildLetters( fs, dt );
		BuildLava( fs, anchor, dt );
		BuildVolume( fs, anchor, eye, pulse, dt );
		BuildProjector( fs, anchor, eye, dt );
		BuildCaustics( fs, anchor, dt );
		BuildLightning( fs, anchor, dt );
		BuildSwarm( fs, anchor, dt );
		BuildPost( fs, anchor, scale );

		BuildWire( fs );
		fs.explode       = values[ P_EXPLODE ] + values[ P_AUDIO_EXPLODE ] * 0.5f * audio[ 2 ];
		fs.explodeRandom = values[ P_EXPLODE_RANDOM ];
		fs.explodeSeed   = 0.37f;
		fs.dissolve      = values[ P_DISSOLVE ];
		fs.dissolveEdge  = values[ P_DISSOLVE_EDGE ];
		fs.toonSteps     = std::floor( values[ P_TOON_STEPS ] + 0.5f );
		fs.outline       = values[ P_OUTLINE ];
		fs.fogDensity    = values[ P_DEPTH_FOG ];
		fs.fogStart      = std::max( 0.0f, dist - scale );

		Colour( P_BG_H, fs.background );
		fs.background[ 3 ] = Alpha( P_BG_H );
		static const int kMsaa[ 3 ] = { 1, 4, 8 };
		fs.msaa = kMsaa[ std::min( 2, std::max( 0, Opt( P_MSAA ) ) ) ];
#if LOADEHR_FX
		fs.inputUse        = Opt( P_INPUT_USE );
		fs.inputProjection = Opt( P_INPUT_PROJECTION );
		fs.inputMix        = values[ P_INPUT_MIX ];
#endif
		fs.debugView = Opt( P_DEBUG_VIEW );
	}

#if LOADEHR_FX
	// ---- Stage 7 face tracking: Vision results -> tracks (nearest-centre association) -> One Euro filter
	// -> one mask instance per face, placed on the plane through the mask's position facing the camera.
	struct OneEuro
	{
		float x = 0, dx = 0;
		bool init = false;
		float Filter( float v, float dt, float minCut, float beta )
		{
			if( !init || dt <= 0.0f )
			{
				init = true, x = v, dx = 0;
				return v;
			}
			auto alpha = [ dt ]( float fc ) { const float tau = 1.0f / ( 6.2831853f * fc ); return 1.0f / ( 1.0f + tau / dt ); };
			dx += alpha( 1.0f ) * ( ( v - x ) / dt - dx );
			x += alpha( minCut + beta * std::fabs( dx ) ) * ( v - x );
			return x;
		}
	};
	struct FaceTrack
	{
		OneEuro f[ 7 ];
		float v[ 7 ] = {};       // filtered cx, cy, w, h, roll, yaw, pitch
		double lastSeen = 0, lastUpdate = 0; // clk.face seconds
		float presence = 0;
	};

	void BuildFaces( FrameState& fs, const Mat4& M, const float* pos, const Mat4& V, const Mat4& P, float dt )
	{
		const bool on = On( P_FACE_TRACK );
		fs.faceWant   = on && !faceTracker.Busy();
		clk.face += dt;
		if( !on )
		{
			faceTracks.clear();
			noFaceAlpha = 1.0f;
			return;
		}
		FaceFrame frame;
		if( faceTracker.TakeLatest( frame ) )
		{
			lastVisionMs = frame.ms;
			if( ++visionResults % 60 == 1 && LogVerbose() )
				Log( "FACE %zu faces, Vision %.1f ms (result %llu)", frame.faces.size(), frame.ms, (unsigned long long)frame.seq );
			const float s = values[ P_FACE_SMOOTH ];
			const float minCut = 8.0f + ( 0.35f - 8.0f ) * s, beta = 0.6f + ( 0.02f - 0.6f ) * s;
			std::vector< bool > used( faceTracks.size(), false );
			for( const FaceResult& r : frame.faces )
			{
				int best      = -1;
				float bestD   = 0.25f; // association radius, normalised image units
				for( size_t i = 0; i < faceTracks.size(); ++i )
				{
					if( used[ i ] )
						continue;
					const float d = std::hypot( faceTracks[ i ].v[ 0 ] - r.cx, faceTracks[ i ].v[ 1 ] - r.cy );
					if( d < bestD )
						bestD = d, best = (int)i;
				}
				if( best < 0 )
				{
					faceTracks.push_back( FaceTrack() );
					used.push_back( false );
					best = (int)faceTracks.size() - 1;
				}
				used[ best ]    = true;
				FaceTrack& t    = faceTracks[ best ];
				const float raw[ 7 ] = { r.cx, r.cy, r.w, r.h, r.roll, r.yaw, r.pitch };
				const float fdt = float( clk.face - t.lastUpdate );
				for( int k = 0; k < 7; ++k )
					t.v[ k ] = t.f[ k ].Filter( raw[ k ], fdt, minCut, beta );
				t.lastUpdate = t.lastSeen = clk.face;
			}
		}
		// presence envelopes: fade in when seen, out 0.4 s after the last sighting
		for( FaceTrack& t : faceTracks )
		{
			const bool seen = clk.face - t.lastSeen < 0.4f;
			t.presence      = std::min( 1.0f, std::max( 0.0f, t.presence + ( seen ? dt : -dt ) / 0.25f ) );
		}
		faceTracks.erase( std::remove_if( faceTracks.begin(), faceTracks.end(), [ & ]( const FaceTrack& t ) { return t.presence <= 0.0f && clk.face - t.lastSeen >= 0.4f; } ),
		                  faceTracks.end() );
		// the largest Max Faces tracks
		std::vector< const FaceTrack* > live;
		for( const FaceTrack& t : faceTracks )
			live.push_back( &t );
		std::sort( live.begin(), live.end(), []( const FaceTrack* a, const FaceTrack* b ) { return a->v[ 2 ] > b->v[ 2 ]; } );
		live.resize( std::min( live.size(), size_t( std::max( 1, (int)std::lround( values[ P_MAX_FACES ] ) ) ) ) );

		const bool hide = On( P_HIDE_NO_FACE );
		noFaceAlpha     = std::min( 1.0f, std::max( 0.0f, noFaceAlpha + ( live.empty() && !hide ? dt : -dt ) / 0.25f ) );
		if( live.empty() )
		{
			if( !hide && noFaceAlpha >= 1.0f )
				return; // nobody there and nothing hidden: the mask stays where it is
			fs.inst.clear();
			Instance in;
			std::memcpy( in.m, M.m, sizeof( in.m ) );
			in.alpha = noFaceAlpha;
			fs.inst.push_back( in );
			return;
		}
		// camera basis and unprojection onto the plane through the mask's position, facing the camera
		const Mat4 inv = ( P * V ).Inverse();
		const Mat4 C   = V.Inverse(); // columns: right, up, back, eye
		const float right[ 3 ] = { C.m[ 0 ], C.m[ 1 ], C.m[ 2 ] }, up[ 3 ] = { C.m[ 4 ], C.m[ 5 ], C.m[ 6 ] }, back[ 3 ] = { C.m[ 8 ], C.m[ 9 ], C.m[ 10 ] };
		auto unproject = [ & ]( float u, float v, float* out ) {
			float a[ 4 ], b[ 4 ];
			const float nx = u * 2.0f - 1.0f, ny = v * 2.0f - 1.0f;
			for( int r = 0; r < 4; ++r )
			{
				a[ r ] = inv.m[ r ] * nx + inv.m[ 4 + r ] * ny - inv.m[ 8 + r ] + inv.m[ 12 + r ];
				b[ r ] = inv.m[ r ] * nx + inv.m[ 4 + r ] * ny + inv.m[ 8 + r ] + inv.m[ 12 + r ];
			}
			float p0[ 3 ], d[ 3 ];
			for( int k = 0; k < 3; ++k )
				p0[ k ] = a[ k ] / a[ 3 ], d[ k ] = b[ k ] / b[ 3 ] - p0[ k ];
			float den = d[ 0 ] * back[ 0 ] + d[ 1 ] * back[ 1 ] + d[ 2 ] * back[ 2 ];
			float num = ( pos[ 0 ] - p0[ 0 ] ) * back[ 0 ] + ( pos[ 1 ] - p0[ 1 ] ) * back[ 1 ] + ( pos[ 2 ] - p0[ 2 ] ) * back[ 2 ];
			const float t = std::fabs( den ) > 1e-6f ? num / den : 0.0f;
			for( int k = 0; k < 3; ++k )
				out[ k ] = p0[ k ] + d[ k ] * t;
		};
		const Mat4 Mloc = Mat4::Translate( -pos[ 0 ], -pos[ 1 ], -pos[ 2 ] ) * M; // the mask's own rotation / scale / fit
		Mat4 Crot       = C;
		Crot.m[ 12 ] = Crot.m[ 13 ] = Crot.m[ 14 ] = 0.0f;
		const Mat4 CrotT = V; // the view's rotation is the transpose of the camera's
		Mat4 CT          = CrotT;
		CT.m[ 12 ] = CT.m[ 13 ] = CT.m[ 14 ] = 0.0f;
		fs.inst.clear();
		for( const FaceTrack* t : live )
		{
			float c[ 3 ], r[ 3 ];
			unproject( t->v[ 0 ], t->v[ 1 ], c );
			unproject( t->v[ 0 ] + t->v[ 2 ] * 0.5f, t->v[ 1 ], r );
			const float faceW = 2.0f * std::sqrt( ( r[ 0 ] - c[ 0 ] ) * ( r[ 0 ] - c[ 0 ] ) + ( r[ 1 ] - c[ 1 ] ) * ( r[ 1 ] - c[ 1 ] ) + ( r[ 2 ] - c[ 2 ] ) * ( r[ 2 ] - c[ 2 ] ) );
			// box height in the same world units: normalised h / w times the viewport's aspect
			const float faceH = faceW * ( t->v[ 2 ] > 1e-4f ? t->v[ 3 ] / t->v[ 2 ] : 1.0f ) * float( fs.height ) / float( std::max( 1, fs.width ) );
			for( int k = 0; k < 3; ++k )
				c[ k ] += right[ k ] * values[ P_FACE_OFFSET_X ] * faceW + up[ k ] * values[ P_FACE_OFFSET_Y ] * faceH;
			const Mat4 R = Crot * Mat4::RotZ( t->v[ 4 ] * values[ P_FOLLOW_ROLL ] ) * Mat4::RotY( t->v[ 5 ] * values[ P_FOLLOW_YAW ] )
			             * Mat4::RotX( -t->v[ 6 ] * values[ P_FOLLOW_PITCH ] ) * CT;
			const Mat4 F = Mat4::Translate( c[ 0 ], c[ 1 ], c[ 2 ] ) * R * Mat4::Scale( faceW * values[ P_FACE_SCALE ] ) * Mloc;
			Instance in;
			std::memcpy( in.m, F.m, sizeof( in.m ) );
			in.alpha = t->presence;
			fs.inst.push_back( in );
		}
	}
#endif

	// ---- Stage 6 poses: a pose is the camera + the mask's placement. Stored in the hidden "Pose Data" text
	// param (value event -> Arena saves it with the clip); Go Pose glides there, then writes the params.
	static constexpr unsigned kPoseIds[ 9 ] = { P_ORBIT_YAW, P_ORBIT_PITCH, P_DISTANCE, P_FOV, P_ROT_X, P_ROT_Y, P_ROT_Z, P_POS_X, P_POS_Y };
	static bool IsAngle( unsigned id ) { return id == P_ORBIT_YAW || id == P_ROT_X || id == P_ROT_Y || id == P_ROT_Z; }

	float PV( unsigned id ) const
	{
		if( glideActive )
			for( int k = 0; k < 9; ++k )
				if( kPoseIds[ k ] == id )
					return glideNow[ k ];
		return values[ id ];
	}

	// Four poses ship with the plugin, so Go Pose 1-4 work on any clip; Store Pose overwrites one.
	// orbit yaw, pitch, distance, FOV, rotate X/Y/Z, pos X/Y
	static constexpr float kDefaultPoses[ 4 ][ 9 ] = {
		{ 0, 0, 4, 35, 0, 0, 0, 0, 0 },          // 1 front
		{ -40, 8, 4, 35, 0, 0, 0, 0, 0 },        // 2 three-quarter left
		{ 15, -22, 3.4f, 40, 0, 0, 0, 0, 0 },    // 3 low hero angle
		{ 0, 55, 4.3f, 35, 0, 0, 0, 0, 0 },      // 4 top-down tilt
	};

	void WritePoseData()
	{
		poseData.clear();
		for( int p = 0; p < 4; ++p )
		{
			char buf[ 256 ];
			int n = snprintf( buf, sizeof( buf ), "%d:", p + 1 );
			for( int k = 0; k < 9; ++k )
				n += snprintf( buf + n, sizeof( buf ) - n, k ? ",%.4f" : "%.4f", poses[ p ][ k ] );
			poseData += std::string( buf ) + ";";
		}
	}

	void StorePose( int i )
	{
		for( int k = 0; k < 9; ++k )
			poses[ i ][ k ] = PV( kPoseIds[ k ] );
		WritePoseData();
		RaiseParamEvent( P_POSE_DATA, FF_EVENT_FLAG_VALUE );
		Log( "Pose %d stored: %s", i + 1, poseData.c_str() );
	}

	// a slot missing from the text (an empty Pose Data, or a clip from before W13) keeps its default pose
	void ParsePoses( const std::string& v )
	{
		std::memcpy( poses, kDefaultPoses, sizeof( poses ) );
		size_t at = 0;
		while( at < v.size() )
		{
			size_t end = v.find( ';', at );
			std::string item = v.substr( at, end == std::string::npos ? std::string::npos : end - at );
			at               = end == std::string::npos ? v.size() : end + 1;
			int p            = 0;
			float f[ 9 ];
			if( sscanf( item.c_str(), "%d:%f,%f,%f,%f,%f,%f,%f,%f,%f", &p, &f[ 0 ], &f[ 1 ], &f[ 2 ], &f[ 3 ], &f[ 4 ], &f[ 5 ], &f[ 6 ], &f[ 7 ], &f[ 8 ] ) == 10 && p >= 1 && p <= 4 )
				std::memcpy( poses[ p - 1 ], f, sizeof( f ) );
		}
		WritePoseData();
	}

	void GoPose( int i )
	{
		for( int k = 0; k < 9; ++k )
		{
			glideFrom[ k ] = PV( kPoseIds[ k ] );
			glideTo[ k ]   = poses[ i ][ k ];
			if( IsAngle( kPoseIds[ k ] ) ) // the short way round
			{
				float d = std::fmod( glideTo[ k ] - glideFrom[ k ] + 540.0f, 360.0f ) - 180.0f;
				glideTo[ k ] = glideFrom[ k ] + d;
			}
			glideNow[ k ] = glideFrom[ k ];
		}
		glideT      = 0.0f;
		glideActive = true;
	}

	// ---- Fit To (W18). The target fit = the model's own fit (bit for bit) when every part counts; otherwise the bbox of
	// the parts whose slot is Visible, centre + largest dimension -> 2 units like the loader's fit. `include` (optional):
	// per part, whether it counts.
	void TargetFit( const GpuModel& m, float* out, std::vector< uint8_t >* include ) const
	{
		const bool visibleOnly = Opt( P_FIT_TO ) == 1;
		float lo[ 3 ] = { 1e30f, 1e30f, 1e30f }, hi[ 3 ] = { -1e30f, -1e30f, -1e30f };
		bool all = true, any = false;
		if( include )
			include->assign( m.parts.size(), 1 );
		for( size_t i = 0; i < m.parts.size(); ++i )
		{
			const GpuPart& p = m.parts[ i ];
			if( p.indexCount == 0 )
				continue;
			if( visibleOnly && !On( PartParam( p.slot, PF_VISIBLE ) ) )
			{
				all = false;
				if( include )
					( *include )[ i ] = 0;
				continue;
			}
			any = true;
			for( int k = 0; k < 3; ++k )
				lo[ k ] = std::min( lo[ k ], p.boxMin[ k ] ), hi[ k ] = std::max( hi[ k ], p.boxMax[ k ] );
		}
		if( all || !any )
		{
			for( int k = 0; k < 3; ++k )
				out[ k ] = m.fitCenter[ k ];
			out[ 3 ] = m.fitScale;
			if( include )
				include->assign( m.parts.size(), 1 );
			return;
		}
		float maxDim = 0;
		for( int k = 0; k < 3; ++k )
			out[ k ] = 0.5f * ( lo[ k ] + hi[ k ] ), maxDim = std::max( maxDim, hi[ k ] - lo[ k ] );
		out[ 3 ] = maxDim > 1e-12f ? 2.0f / maxDim : m.fitScale;
	}

	// the fit in use glides to the target over 0.5 s (smoothstep; the scale in log space); a new model snaps. One per slot.
	void UpdateFit( int slot, const GpuModel& m, float dt )
	{
		float* fitNow  = this->fitNow[ slot ];
		float* fitFrom = this->fitFrom[ slot ];
		float* fitTo   = this->fitTo[ slot ];
		float& fitT    = this->fitT[ slot ];
		float target[ 4 ];
		TargetFit( m, target, nullptr );
		if( m.serial != fitSerial[ slot ] )
		{
			fitSerial[ slot ] = m.serial;
			std::memcpy( fitNow, target, sizeof( target ) );
			std::memcpy( fitTo, target, sizeof( target ) );
			fitT = 1;
			return;
		}
		if( std::memcmp( target, fitTo, sizeof( target ) ) != 0 )
		{
			std::memcpy( fitFrom, fitNow, sizeof( target ) );
			std::memcpy( fitTo, target, sizeof( target ) );
			fitT = 0;
			if( On( P_DEBUG_LOG ) )
				Log( "FIT %c glide to centre (%.4g %.4g %.4g) scale %.4g", "ABCD"[ slot ], target[ 0 ], target[ 1 ], target[ 2 ], target[ 3 ] );
		}
		if( fitT >= 1 )
			return;
		fitT = std::min( 1.0f, fitT + dt / 0.5f );
		const float u = fitT * fitT * ( 3.0f - 2.0f * fitT );
		for( int k = 0; k < 3; ++k )
			fitNow[ k ] = fitFrom[ k ] + ( fitTo[ k ] - fitFrom[ k ] ) * u;
		fitNow[ 3 ] = std::exp( std::log( fitFrom[ 3 ] ) + ( std::log( fitTo[ 3 ] ) - std::log( fitFrom[ 3 ] ) ) * u );
		if( fitT >= 1 )
			std::memcpy( fitNow, fitTo, sizeof( target ) );
	}

	void UpdateGlide( float dt )
	{
		if( !glideActive )
			return;
		const float dur = values[ P_POSE_GLIDE ];
		glideT += dt;
		float u = dur <= 1e-3f ? 1.0f : std::min( 1.0f, glideT / dur );
		u       = u * u * ( 3.0f - 2.0f * u );
		for( int k = 0; k < 9; ++k )
			glideNow[ k ] = glideFrom[ k ] + ( glideTo[ k ] - glideFrom[ k ] ) * u;
		if( u >= 1.0f )
		{
			// land: write the pose into the params so Arena shows (and saves) where the camera is
			glideActive = false;
			for( int k = 0; k < 9; ++k )
			{
				const ParamDef* d = nullptr;
				for( const ParamDef& p : ParamDefs() )
					if( p.id == kPoseIds[ k ] )
						d = &p;
				float v = glideTo[ k ];
				if( IsAngle( kPoseIds[ k ] ) )
					v = std::fmod( v + 540.0f, 360.0f ) - 180.0f;
				values[ kPoseIds[ k ] ] = d ? std::min( d->max, std::max( d->min, v ) ) : v;
				RaiseParamEvent( kPoseIds[ k ], FF_EVENT_FLAG_VALUE );
			}
		}
	}

	// beat cuts (hard cuts between a fixed set of angles around the base camera) + beat shake
	void UpdateCameraLive( float& yaw, float& pitch, float& dist, float& roll, float dt )
	{
		static const float kYaw[ 8 ] = { 0, -1, 1, -0.45f, 0.55f, -0.8f, 0.3f, 0.9f };
		static const float kPitch[ 8 ] = { 0, 0.25f, -0.2f, 0.55f, -0.45f, -0.1f, 0.7f, 0.35f };
		static const float kDist[ 8 ] = { 0, -0.25f, 0.15f, 0.3f, -0.15f, 0.45f, -0.3f, 0.05f };
		const int every = Opt( P_CUT_EVERY ) == 0 ? 0 : 1 << ( Opt( P_CUT_EVERY ) - 1 );
		const int n     = std::max( 2, std::min( 8, (int)std::lround( values[ P_CUT_ANGLES ] ) ) );
		if( every == 0 )
			cutIndex = 0;
		else if( beatCount != lastCutBeat && beatCount % every == 0 )
		{
			lastCutBeat = beatCount;
			if( Opt( P_CUT_ORDER ) == 1 )
				cutIndex = ( cutIndex + 1 ) % n;
			else
			{
				cutRng   = cutRng * 1664525u + 1013904223u;
				int next = int( ( cutRng >> 8 ) % uint32_t( n - 1 ) );
				cutIndex = next >= cutIndex ? next + 1 : next; // never the same angle twice
			}
		}
		cutIndex = std::min( cutIndex, n - 1 );
		const float spread = values[ P_ANGLE_SPREAD ];
		yaw += kYaw[ cutIndex ] * spread;
		pitch += kPitch[ cutIndex ] * spread * 0.4f;
		dist *= 1.0f + kDist[ cutIndex ] * std::min( 1.0f, spread / 90.0f );
		// shake: a kick on every beat, decaying with Shake Decay (seconds)
		if( beatCount != lastShakeBeat )
			shakeEnv = std::max( shakeEnv, values[ P_BEAT_SHAKE ] ), lastShakeBeat = beatCount;
		shakeEnv *= std::exp( -dt / std::max( 0.02f, values[ P_SHAKE_DECAY ] ) );
		clk.shake = Wrap( clk.shake + dt, kSmoothPeriod ); // read at whole multiples (x17, x19, x23, x29)
		if( shakeEnv > 1e-4f )
		{
			yaw += shakeEnv * 3.0f * Smooth1D( clk.shake * 23.0, 1.0f );
			pitch += shakeEnv * 3.0f * Smooth1D( clk.shake * 19.0, 4.0f );
			roll += shakeEnv * 2.0f * Smooth1D( clk.shake * 17.0, 7.0f );
			dist *= 1.0f + shakeEnv * 0.03f * Smooth1D( clk.shake * 29.0, 9.0f );
		}
	}

	// ---- Hue Drift (W15). A non-zero rate integrates the light's hue offset (turns, wrapped at 1). At 0 (the dial shows
	// 0.00) the offset glides home the short way round the hue circle over Drift Return seconds (smoothstep), so the
	// light ends exactly on the picked colour; drifting again continues from wherever the glide is. The picker stays
	// the anchor: the offset is added to it. Offsets: 0 A, 1 B, 2 C (both sides), 3 spots (Beat Strobe steps: `step`
	// is added this frame, `hold` keeps the offset while the strobe is on).
	void Drift( int i, float rate, float dt, double step = 0.0, bool hold = false )
	{
		static const char* kName[ 4 ] = { "A", "B", "C", "Spots" };
		double& ph = clk.lightHue[ i ];
		float& t   = driftReturnT[ i ];
		if( std::fabs( rate ) >= 0.005f || hold )
		{
			ph = Wrap( ph + rate * dt + step, 1.0 );
			t  = -1.0f;
			return;
		}
		double off = ph > 0.5 ? ph - 1.0 : ph; // signed: the short way home
		if( off == 0.0 )
			return;
		const float ret = values[ P_DRIFT_RETURN ];
		if( t < 0.0f )
		{
			t = 0.0f;
			if( LogVerbose() )
				Log( "DRIFT %s: Hue Drift 0, offset %+.3f turns glides home over %.2f s", kName[ i ], off, ret );
		}
		const float u0 = ret > 1e-3f ? SmoothStep( 0.0f, ret, t ) : 1.0f;
		t += dt;
		const float u1 = ret > 1e-3f ? SmoothStep( 0.0f, ret, t ) : 1.0f;
		off            = u1 >= 1.0f || u0 >= 1.0f ? 0.0 : off * ( 1.0 - u1 ) / ( 1.0 - u0 ); // continuous if Drift Return moves mid-glide
		ph             = off < 0.0 ? off + 1.0 : off;
		if( off == 0.0 && LogVerbose() )
			Log( "DRIFT %s: home after %.2f s (on the picked colour)", kName[ i ], t );
		else if( LogVerbose() && int( ( t - dt ) * 10.0f ) != int( t * 10.0f ) )
			Log( "DRIFT %s: offset %+.4f turns at %.2f s", kName[ i ], off, t );
	}

	// ---- volume lights A/B + the smoky medium
	void BuildVolume( FrameState& fs, const Mat4& anchor, const float* eye, float pulse, float dt )
	{
		static const unsigned bases[ 2 ] = { P_LA_ENABLE, P_LB_ENABLE };
		auto F = [ & ]( int L, unsigned field ) { return bases[ L ] + ( field - P_LA_ENABLE ); };
		float center[ 3 ];
		const float origin[ 3 ] = { 0, 0, 0 };
		anchor.Point( origin, center );
		int n = 0;
		for( int L = 0; L < 2; ++L )
		{
			Drift( L, values[ F( L, P_LA_HUE_DRIFT ) ], dt );
			if( !On( F( L, P_LA_ENABLE ) ) )
				continue;
			XLight& x          = fs.xl[ n++ ];
			x.type             = Opt( F( L, P_LA_TYPE ) );
			const bool anchored = On( F( L, P_LA_ANCHOR ) );
			const float lp[ 3 ] = { values[ F( L, P_LA_POS_X ) ], values[ F( L, P_LA_POS_Y ) ], values[ F( L, P_LA_POS_Z ) ] };
			if( anchored )
				anchor.Point( lp, x.pos );
			else
				std::memcpy( x.pos, lp, sizeof( lp ) );
			float d[ 3 ];
			const int aim = Opt( F( L, P_LA_AIM_MODE ) );
			if( aim == 0 || aim == 1 )
			{
				const float* target = aim == 0 ? eye : center;
				for( int k = 0; k < 3; ++k )
					d[ k ] = target[ k ] - x.pos[ k ];
			}
			else
			{
				const float yaw = values[ F( L, P_LA_AIM_YAW ) ] * kDeg, pitch = values[ F( L, P_LA_AIM_PITCH ) ] * kDeg;
				const float local[ 3 ] = { std::cos( pitch ) * std::sin( yaw ), std::sin( pitch ), std::cos( pitch ) * std::cos( yaw ) };
				if( anchored )
					anchor.Dir( local, d );
				else
					std::memcpy( d, local, sizeof( local ) );
			}
			float len = std::sqrt( d[ 0 ] * d[ 0 ] + d[ 1 ] * d[ 1 ] + d[ 2 ] * d[ 2 ] );
			if( len < 1e-5f )
				d[ 0 ] = 0, d[ 1 ] = 0, d[ 2 ] = 1, len = 1;
			for( int k = 0; k < 3; ++k )
				x.dir[ k ] = d[ k ] / len;
			float c[ 3 ];
			ColourDrift( F( L, P_LA_COL_H ), float( clk.lightHue[ L ] ), c );
			const float gain = values[ F( L, P_LA_INTENSITY ) ] * Alpha( F( L, P_LA_COL_H ) ) * ( 1.0f + values[ F( L, P_LA_BEAT ) ] * pulse ) * ( 1.0f + values[ F( L, P_LA_AUDIO ) ] * audio[ 0 ] );
			for( int k = 0; k < 3; ++k )
				x.color[ k ] = std::max( 0.0f, c[ k ] ) * gain;
			x.coneDeg  = values[ F( L, P_LA_CONE ) ];
			x.coneSoft = values[ F( L, P_LA_CONE_SOFT ) ];
			x.shadow   = true;
		}
		// W26 Lava Light: a point light inside the split mask (a volume light: its cube shadow lets it out through the gaps only);
		// it needs one of the two cube-shadow slots (A / B first)
		if( fs.lava.on && fs.lava.split > 0.0f && values[ P_LAVA_LIGHT ] > 0.0f && n < 2 )
		{
			XLight& x = fs.xl[ n++ ];
			x.type = 1, x.shadow = true;
			anchor.Point( origin, x.pos );
			float bb[ 3 ];
			Blackbody( 0.9f * fs.lava.temp, bb );
			const float k = values[ P_LAVA_LIGHT ] * fs.lava.split * std::min( 1.0f, fs.lava.front / 1.5f );
			for( int c = 0; c < 3; ++c )
				x.color[ c ] = ( bb[ c ] + ( fs.lava.tint[ c ] - bb[ c ] ) * fs.lava.tintMix ) * k;
		}
		BuildEyeLights( fs, n, anchor, eye, pulse, dt );
		const int volLights = n;
		n                   = BuildSpots( fs, n, center, pulse, dt );
		fs.xlCount          = n;
		VolumeState& v      = fs.vol;
		v.lights            = volLights;
		v.spotBeams         = values[ P_SPOT_BEAMS ];
		v.fogLights         = v.spotBeams > 0.0f ? n : volLights;
		v.smokeAmbient      = values[ P_SMOKE_AMBIENT ];
		v.smokeKey          = values[ P_SMOKE_KEY ];
		BuildTrip( fs, anchor, dt );
		const TripState& tr = fs.trip; // W24: smoke that glows by itself needs no lamp
		const bool selfLit  = tr.veins > 0.0f || tr.contours > 0.0f || tr.lines > 0.0f || tr.black > 0.0f || values[ P_FIRE ] > 0.0f; // W26: flames glow
		v.active            = v.fogLights > 0 || ( values[ P_SMOKE_AMOUNT ] > 0.0f && ( v.smokeAmbient > 0.0f || v.smokeKey > 0.0f || selfLit ) );
		v.method       = Opt( P_VOL_METHOD );
		v.haze         = values[ P_HAZE ];
		v.smoke        = values[ P_SMOKE_AMOUNT ];
		v.smokeFreq    = values[ P_SMOKE_SCALE ] * 0.35f;
		v.turb         = values[ P_SMOKE_TURB ];
		v.glowCloud    = values[ P_GLOW_CLOUD ];
		// the smoke field is translated in noise space (integrated, so dragging a speed never jumps it)
		const float speed = values[ P_SMOKE_SPEED ], rise = values[ P_SMOKE_RISE ];
		const double rate[ 3 ] = { speed * 0.6, rise, speed * 0.25 };
		const float warpDt     = dt * TimeWarp(); // W24 Time Warp / Freeze: the noise smoke drifts at the sim's pace
		for( int k = 0; k < 3; ++k )
		{
			// the noise texture tiles every 1.0 and is read at x1, x0.43 and x2.63: 100 is a whole number of tiles for all three
			clk.smoke[ k ] = Wrap( clk.smoke[ k ] - warpDt * v.smokeFreq * rate[ k ], 100.0 );
			v.drift[ k ]   = float( clk.smoke[ k ] );
			v.center[ k ]  = center[ k ];
		}
		clk.vol = Wrap( clk.vol + dt, kMoteLoop ); // motes: every rate is a whole number of cycles per loop
		v.time  = float( clk.vol );
		v.radius   = values[ P_VOLUME_SIZE ];
		v.g        = values[ P_SCATTER_G ];
		v.shaftLen = values[ P_SHAFT_LENGTH ];
		v.motes    = values[ P_MOTES ];
		static const int kSteps[ 4 ] = { 16, 32, 64, 128 }, kDiv[ 3 ] = { 1, 2, 4 };
		v.steps  = kSteps[ std::min( 3, std::max( 0, Opt( P_VOL_QUALITY ) ) ) ];
		v.resDiv = kDiv[ std::min( 2, std::max( 0, Opt( P_VOL_RES ) ) ) ];
		v.blend  = Opt( P_FOG_BLEND );
		BuildSim( fs, anchor, eye, dt );
	}

	// ---- W20 smoke sim: the grid follows the mask (position, not rotation); emitters in world space
	void BuildSim( FrameState& fs, const Mat4& anchor, const float* eye, float dt )
	{
		SimState& m = fs.sim;
		m.engine    = Opt( P_SMOKE_ENGINE );
		if( m.engine == 0 && values[ P_FIRE ] > 0.0f )
			m.engine = 1; // W26: fire burns in the sim
		if( m.engine == 0 )
		{
			simPrevValid = false;
			return;
		}
		static const int kRes[ 4 ] = { 48, 64, 96, 128 };
		m.res = kRes[ std::clamp( Opt( P_SIM_RES ), 0, 3 ) ];
		const float o[ 3 ] = { 0, 0, 0 }, ux[ 3 ] = { 1, 0, 0 };
		float wx[ 3 ];
		anchor.Point( o, m.boxCenter );
		anchor.Dir( ux, wx );
		const float unit = std::sqrt( wx[ 0 ] * wx[ 0 ] + wx[ 1 ] * wx[ 1 ] + wx[ 2 ] * wx[ 2 ] ); // world length of one mask unit
		m.boxSize        = values[ P_SIM_BOX ] * 2.0f * std::max( unit, 1e-3f );                  // a fitted mask is 2 units
		m.unit           = std::max( unit, 1e-3f );
		// LoadEhr W1: the obstacle is the chain's dominant model (its matrix). When it changes (a hop's midpoint) the new obstacle
		// starts with no velocity of its own: last frame's matrix belonged to the other model, and p - prev * raw across two fits
		// would kick the smoke in one frame. So the switch is a still obstacle (no pop), then it moves with the mask again.
		if( domSlot != simDomSlot )
			simPrevValid = false, simDomSlot = domSlot;
		std::memcpy( m.model, domM.m, sizeof( m.model ) );
		std::memcpy( m.prevModel, simPrevValid ? simPrevModel : domM.m, sizeof( m.prevModel ) );
		std::memcpy( simPrevModel, domM.m, sizeof( simPrevModel ) );
		simPrevValid = true;
		m.frameDt    = std::max( dt, 1e-3f );
		m.iterations = std::clamp( (int)std::lround( values[ P_SIM_ITER ] ), 10, 40 );
		m.buoyancy   = values[ P_BUOYANCY ] * unit;
		m.weight     = 0.15f * unit;
		m.heatDecay  = values[ P_HEAT_DECAY ];
		m.vorticity  = values[ P_VORTICITY ];
		m.fade       = values[ P_SMOKE_FADE ];
		m.turb       = values[ P_SIM_TURB ] * unit;
		for( int k = 0; k < 3; ++k )
			m.wind[ k ] = values[ P_WIND_X + k ] * unit;
		// eyes: the Light C openings (placed by the eye finder, or by hand), pushing out of the mask's front
		const bool anchored = On( P_LC_ANCHOR );
		for( int side = 0; side < 2; ++side )
		{
			float open[ 3 ];
			EyeOpen( side, open ); // Light C's opening (+ the chain's eye offset)
			open[ 2 ] -= 0.04f; // just inside the opening
			if( anchored )
				anchor.Point( open, &m.eyePos[ side * 3 ] );
			else
				std::memcpy( &m.eyePos[ side * 3 ], open, sizeof( open ) );
		}
		const float fz[ 3 ] = { 0, 0, 1 };
		anchor.Dir( fz, m.eyeDir );
		Mat4::Norm( m.eyeDir );
		m.emitOffset = values[ P_EMIT_OFFSET ] * unit;
		for( int side = 0; side < 2; ++side )
			for( int k = 0; k < 3; ++k )
				m.eyePos[ side * 3 + k ] += m.eyeDir[ k ] * m.emitOffset;
		m.eyeRad     = 0.1f * unit;
		m.eyeSpeed   = 1.2f * unit;
		m.emitEyes   = values[ P_EMIT_EYES ];
		m.eyesHeat   = values[ P_EYES_HEAT ];
		const float back[ 3 ] = { 0, 0, -0.6f };
		anchor.Point( back, m.behindPos );
		m.behindRad  = 0.45f * unit;
		m.emitBehind = values[ P_EMIT_BEHIND ];
		m.behindHeat = values[ P_BEHIND_HEAT ];
		m.emitRim    = values[ P_EMIT_RIM ];
		m.rimHeat    = values[ P_RIM_HEAT ];
		m.emitInside = values[ P_EMIT_INSIDE ];
		m.insideHeat = values[ P_INSIDE_HEAT ];
		// Beat Puff: a burst from every active emitter on the chosen beats (0.1 s attack-free decay), x Audio > Emit
		const int every = Opt( P_BEAT_PUFF ) == 0 ? 0 : 1 << ( Opt( P_BEAT_PUFF ) - 1 );
		if( every > 0 && beatCount != lastPuffBeat && beatCount % every == 0 )
			lastPuffBeat = beatCount, puffT = 0.0f;
		puffT     = std::min( puffT + dt, 10.0f );
		m.emitMul = ( 1.0f + ( every > 0 ? values[ P_PUFF_AMOUNT ] * std::exp( -puffT * 10.0f ) * 4.0f : 0.0f ) ) * ( 1.0f + values[ P_AUDIO_EMIT ] * audio[ 0 ] );
		m.confine = values[ P_SMOKE_CONFINE ];
		std::memcpy( m.confineCenter, m.boxCenter, sizeof( m.boxCenter ) );
		m.confineR = 1.3f * unit;
		Colour( P_SMOKE_COL_H, m.tint );
		fs.vol.smoke *= Alpha( P_SMOKE_COL_H ); // the sim smoke's density (the Noise engine never gets here)
#if LOADEHR_FX
		m.project = values[ P_SIM_PROJECT ];
#else
		m.project = 0.0f;
#endif
		m.reset         = simResetRequest;
		simResetRequest = false;
		BuildSimTrip( fs, anchor, dt );
		BuildFire( fs, dt );
		// the march must reach the whole grid
		fs.vol.radius = std::max( fs.vol.radius, m.boxSize * 0.87f );
		(void)eye;
	}

	// ---- W26 Stage 3: fire. The FIRE sim passes run while Fire > 0 (b = fuel). Fire Burst: a fuel surge on the beat. The
	// flames light the mask through one more point light (surface only, appended after the spots): placed at the emitters'
	// weighted centre lifted by the flame height, flickering (Smooth1D at x9 / x23: the clock wraps at kSmoothPeriod).
	// It is estimated from the emitters, not read back from the GPU (readback = debug only).
	void BuildFire( FrameState& fs, float dt )
	{
		SimState& m    = fs.sim;
		const float f  = values[ P_FIRE ];
		const int every = Opt( P_FIRE_BURST ) == 0 ? 0 : 1 << ( Opt( P_FIRE_BURST ) - 1 );
		if( every > 0 && beatCount != lastBurstBeat && beatCount % every == 0 )
			lastBurstBeat = beatCount, burstT = 0.0f;
		const float pace = values[ P_FIRE_SPEED ]; // W28: bursts decay and the light flickers at the fire's pace
		burstT    = std::min( burstT + dt * pace, 10.0f );
		clk.fire  = Wrap( clk.fire + dt * pace, kSmoothPeriod );
		m.fire    = f > 0.0f;
		if( !m.fire )
			return;
		const float burst = every > 0 ? values[ P_BURST_AMOUNT ] * std::exp( -burstT * 5.0f ) : 0.0f;
		const float h     = values[ P_FLAME_HEIGHT ];
		m.fireFuel     = f * 1.5f * ( 1.0f + 0.5f * burst );
		m.fireSmoke    = 1.0f - f;
		m.burn         = values[ P_BURN_RATE ];
		m.heatRelease  = 8.0f;
		m.soot         = values[ P_FIRE_SMOKE ] * 0.6f;
		m.fireCool     = 1.2f / ( 0.3f + h );
		m.lift         = ( 0.5f + 1.5f * h ) * m.unit;
		m.fuelFade     = 0.3f;
		m.flameTemp    = values[ P_FLAME_TEMP ];
		m.flameGlow    = values[ P_FLAME_GLOW ];
		m.flameTintMix = Alpha( P_FLAME_TINT_H );
		m.flameSoot    = 0.3f;
		Colour( P_FLAME_TINT_H, m.flameTint );
		// the flicker light: the emitters' centre, lifted
		float c[ 3 ] = {}, wsum = 0;
		auto add = [ & ]( const float* p, float w ) {
			for( int k = 0; k < 3; ++k )
				c[ k ] += p[ k ] * w;
			wsum += w;
		};
		add( &m.eyePos[ 0 ], m.emitEyes ), add( &m.eyePos[ 3 ], m.emitEyes ), add( m.behindPos, m.emitBehind );
		add( m.maskCenter, m.emitRim + m.emitInside );
		const float power = values[ P_FIRE_LIGHT ] * f * std::min( 1.0f, wsum / 6.0f ) * ( 1.0f + 0.5f * burst );
		if( wsum <= 0.0f || power <= 0.0f || fs.xlCount >= kMaxXLights )
			return;
		const float flick = 1.0f + 0.25f * Smooth1D( clk.fire * 9.0, 1.7f ) + 0.12f * Smooth1D( clk.fire * 23.0, 5.3f );
		XLight& x = fs.xl[ fs.xlCount++ ];
		x.type    = 1;
		x.shadow  = false;
		for( int k = 0; k < 3; ++k )
			x.pos[ k ] = c[ k ] / wsum + m.maskUp[ k ] * ( 0.25f + 0.25f * h ) * m.unit;
		float bb[ 3 ];
		Blackbody( 0.8f * m.flameTemp, bb );
		for( int k = 0; k < 3; ++k )
			x.color[ k ] = ( bb[ k ] + ( m.flameTint[ k ] - bb[ k ] ) * m.flameTintMix ) * power * flick * 3.0f;
	}
	// ---- LoadEhr W1: the MORPH CHAIN (generalises MaskEhr's Model B + Transition). The filled slots in order (A, B, C, D; empty
	// or failed ones skipped) are the chain, H hops between them (Chain Loop: + the hop last -> first). A hop morphs the outgoing
	// model (cur) into the incoming one (next) at progress t; at a whole point the picture is one pure model.
	struct Chain
	{
		int n = 0, filled[ kModelSlots ] = {}, hops = 0;
		int hop = 0, cur = -1, next = -1; // next = -1: resting on cur, no hop
		float t     = 0;
		int style   = 0;
	};
	Chain chain;
	bool SlotFilled( int i ) const
	{
		const ModelSlot& m = slots[ i ];
		return !m.file.empty() && ( m.current || m.pending || m.ticket );
	}
	// the style of hop h: Fixed = Transition Style; Cycle = Transition Style + h (round the five); Random = a hash of h (stable
	// while scrubbing, the same hop always morphs the same way)
	int HopStyle( int h ) const
	{
		const int base = std::clamp( Opt( P_TRANSITION_STYLE ), 0, 4 ), order = Opt( P_STYLE_ORDER );
		if( order == 1 )
			return ( base + h ) % 5;
		if( order == 2 )
		{
			uint32_t x = uint32_t( h + 1 ) * 2654435761u;
			x ^= x >> 15, x *= 2246822519u, x ^= x >> 13;
			return int( x % 5u );
		}
		return base;
	}
	// which per-slot caches the chain's hops need ahead of use (the styles any hop can take)
	unsigned ChainUses() const
	{
		if( chain.hops == 0 )
			return 0;
		const int order = Opt( P_STYLE_ORDER );
		const int style = std::clamp( Opt( P_TRANSITION_STYLE ), 0, 4 );
		auto use = []( int st ) { return st == 1 ? (unsigned)kUseShards : st == 2 ? (unsigned)kUseDust : st == 4 ? (unsigned)kUseLiquid : 0u; };
		// the liquid SDF is built for every filled slot whatever the style (CPU, off the render thread; 1 MB GPU each), so switching
		// the style to Liquid mid-show finds every hop ready
		return ( order == 0 ? use( style ) : ( kUseShards | kUseDust ) ) | kUseLiquid;
	}

	// W28 Transition Play over the chain: restarted by a clip trigger (WatchTrigger) and by switching Play on. Each hop = Transition
	// Time of morph (smoothstepped: it leaves one model and lands on the next gently, Trans Ease shapes Shatter / Dust inside it) and
	// Hold Time resting on the model it arrived at. The clock holds while the hop's incoming model is not on the GPU yet.
	// Clock (W14): double, wrapped. Once holds at the end of the last hop; Ping-Pong wraps at 2 x H hops (there and back, every
	// model rests Hold Time on each arrival); Loop wraps at H hops with Chain Loop (seamless: the last hop lands on the first model),
	// else at the last arrival + max(Hold Time, MaskEhr's end hold min(1.5 s, Time / 4)), then starts again from the first model.
	// 2 slots, Hold 0 = MaskEhr's Transition Play exactly.
	double ChainPosition( const Chain& c, float dt )
	{
		const int play = Opt( P_TRANS_PLAY );
		if( play != lastTransPlay )
			transRestart = transRestart || play != 0, lastTransPlay = play;
		if( c.hops == 0 )
			return 0.0;
		if( play == 0 )
			return double( values[ P_TRANSITION ] ) * c.hops;
		const double T = std::max( 0.5f, values[ P_TRANS_TIME ] ), Hd = std::max( 0.0f, values[ P_HOLD_TIME ] ), seg = T + Hd;
		const int H    = c.hops;
		// position along a walk of `hops` forward hops ([morph T][hold Hd] each), 0..hops
		auto walk = [ & ]( double tau ) {
			const int k = (int)std::floor( tau / seg );
			if( k >= H )
				return double( H );
			const double u = std::min( 1.0, ( tau - k * seg ) / T );
			return k + double( SmoothStep( 0.0f, 1.0f, float( u ) ) );
		};
		auto position = [ & ]( double tau ) {
			if( play == 2 )
			{
				// Ping-Pong: hops 0 .. 2H - 1, the second half walks back
				const int j = std::min( 2 * H - 1, (int)std::floor( tau / seg ) );
				if( j < H )
					return walk( tau );
				const double u = std::min( 1.0, ( tau - j * seg ) / T );
				return double( 2 * H - j ) - double( SmoothStep( 0.0f, 1.0f, float( u ) ) );
			}
			return walk( tau );
		};
		const double endHold = std::max( Hd, std::min( 1.5, 0.25 * T ) );
		const double period  = play == 1 ? H * seg - Hd : play == 2 ? 2.0 * H * seg : On( P_CHAIN_LOOP ) ? H * seg : H * seg - Hd + endHold;
		// hold the clock while the hop it is in lacks a model on the GPU (it would show a half-loaded one)
		const double x0 = position( clk.transPlay );
		const int h0    = std::clamp( (int)std::floor( x0 ), 0, H - 1 );
		const bool pairReady = slots[ c.filled[ h0 % c.n ] ].current && slots[ c.filled[ ( h0 + 1 ) % c.n ] ].current;
		if( transRestart )
		{
			if( !slots[ c.filled[ 1 % c.n ] ].current )
				return 0.0; // the restart waits for the first hop's model (MaskEhr: until Model B is on the GPU)
			clk.transPlay = 0.0, transRestart = false;
		}
		else if( pairReady )
			clk.transPlay = play == 1 ? std::min( clk.transPlay + dt, period ) : Wrap( clk.transPlay + dt, period );
		return position( clk.transPlay );
	}

	void EvalChain( float dt )
	{
		Chain c;
		for( int i = 0; i < kModelSlots; ++i )
			if( SlotFilled( i ) )
				c.filled[ c.n++ ] = i;
		c.hops = c.n < 2 ? 0 : On( P_CHAIN_LOOP ) ? c.n : c.n - 1;
		const double x = ChainPosition( c, dt );
		if( c.n == 0 )
		{
			chain = c;
			return;
		}
		int h = std::clamp( (int)std::floor( x ), 0, std::max( 0, c.hops - 1 ) );
		float t = float( std::clamp( x - h, 0.0, 1.0 ) );
		if( c.hops > 0 && t >= 1.0f )
		{
			// a whole point: rest on the model there as the next hop's outgoing model (the end of an open chain: the last one)
			++h, t = 0.0f;
		}
		c.hop  = h;
		c.t    = t;
		c.cur  = c.filled[ h % c.n ];
		c.next = c.hops > 0 && h < c.hops ? c.filled[ ( h + 1 ) % c.n ] : -1;
		c.style = c.hops > 0 ? HopStyle( std::min( h, c.hops - 1 ) ) : 0;
		if( ( c.cur != chain.cur || c.next != chain.next ) && LogVerbose() )
			Log( "CHAIN hop %d/%d %c -> %c (%s), position %.3f", h, c.hops, c.cur >= 0 ? "ABCD"[ c.cur ] : '-', c.next >= 0 ? "ABCD"[ c.next ] : '-',
			     ParamDefs()[ P_TRANSITION_STYLE ].options[ size_t( c.style ) ].c_str(), x );
		chain = c;
	}

	// kelvin -> linear rgb, max 1 (Helland's fit, as the shaders' Blackbody)
	static void Blackbody( float k, float* rgb )
	{
		const float t = std::min( 15000.0f, std::max( 800.0f, k ) ) / 100.0f;
		float r = t <= 66.0f ? 1.0f : std::clamp( 1.292936f * std::pow( t - 60.0f, -0.1332047592f ), 0.0f, 1.0f );
		float g = t <= 66.0f ? std::clamp( 0.3900815788f * std::log( t ) - 0.6318414438f, 0.0f, 1.0f ) : std::clamp( 1.129890861f * std::pow( t - 60.0f, -0.0755148492f ), 0.0f, 1.0f );
		float b = t >= 66.0f ? 1.0f : t <= 19.0f ? 0.0f : std::clamp( 0.5432067891f * std::log( t - 10.0f ) - 1.19625408914f, 0.0f, 1.0f );
		rgb[ 0 ] = std::pow( r, 2.2f ), rgb[ 1 ] = std::pow( g, 2.2f ), rgb[ 2 ] = std::pow( b, 2.2f );
	}

	// ---- W24 Trip. Time Warp / Freeze (hold) are not scaled by Trip Master: they are a speed, not a look.
	float TimeWarp() const { return values[ P_FREEZE ] > 0.5f ? 0.0f : values[ P_TIME_WARP ]; }

	void BuildTrip( FrameState& fs, const Mat4& anchor, float dt )
	{
		TripState& t   = fs.trip;
		const float m  = values[ P_TRIP_MASTER ];
		const float o[ 3 ] = { 0, 0, 0 }, ux[ 3 ] = { 1, 0, 0 }, uy[ 3 ] = { 0, 1, 0 }, uz[ 3 ] = { 0, 0, 1 };
		anchor.Point( o, t.center );
		anchor.Dir( ux, t.right );
		anchor.Dir( uy, t.up );
		anchor.Dir( uz, t.axis );
		t.unit = std::max( 1e-3f, std::sqrt( t.right[ 0 ] * t.right[ 0 ] + t.right[ 1 ] * t.right[ 1 ] + t.right[ 2 ] * t.right[ 2 ] ) );
		Mat4::Norm( t.right ), Mat4::Norm( t.up ), Mat4::Norm( t.axis );
		// clocks (W14): palette turns wrap at 1 (the palettes are periodic in 1), kaleido / fractal angles at 360, band scroll at 1
		clk.palette = Wrap( clk.palette + dt * values[ P_PALETTE_SHIFT ], 1.0 );
		clk.kaleido = Wrap( clk.kaleido + dt * values[ P_KALEIDO_SPIN ], 360.0 );
		clk.fractal = Wrap( clk.fractal + dt * values[ P_FRACTAL_MORPH ], 360.0 );
		clk.band    = Wrap( clk.band + dt * values[ P_BAND_SCROLL ], 1.0 );
		t.colourBy  = Opt( P_COLOUR_BY );
		t.palette   = std::clamp( Opt( P_TRIP_PALETTE ), 0, 5 );
		t.palMix    = t.colourBy > 0 ? m : 0.0f;
		t.palShift  = float( clk.palette );
		t.palSpread = values[ P_PALETTE_SPREAD ];
		Colour( P_TRIP_A_H, t.palA );
		Colour( P_TRIP_B_H, t.palB );
		for( int k = 0; k < 3; ++k )
			t.palA[ k ] *= Alpha( P_TRIP_A_H ), t.palB[ k ] *= Alpha( P_TRIP_B_H );
		t.film       = values[ P_THIN_FILM ] * m;
		t.filmThick  = values[ P_THIN_FILM_THICK ];
		t.prism      = values[ P_PRISM ] * m;
		t.prismAngle = values[ P_PRISM_ANGLE ] * float( kTwoPi / 360.0 );
		t.kaleido    = m > 0.0f ? std::clamp( Opt( P_KALEIDO ), 1, 16 ) : 1;
		t.kTwist     = values[ P_KALEIDO_TWIST ] * float( kTwoPi / 360.0 ) / t.unit;
		t.kSpin      = float( clk.kaleido * kTwoPi / 360.0 );
		t.fractal    = values[ P_FRACTAL_FOG ] * m;
		t.fracScale  = values[ P_FRACTAL_SCALE ] * t.unit;
		t.fracIter   = std::clamp( Opt( P_FRACTAL_ITER ), 1, 6 );
		{
			// the folds turn round a tilted axis (identity at 0: the plain Menger lattice)
			const float a = float( clk.fractal * kTwoPi / 360.0 ), c = std::cos( a ), sn = std::sin( a );
			float ax[ 3 ] = { 0.3f, 1.0f, 0.5f };
			Mat4::Norm( ax );
			for( int i = 0; i < 3; ++i )
				for( int j = 0; j < 3; ++j )
				{
					const int e   = 3 - i - j; // the remaining axis for the cross-product term
					float cross   = 0;
					if( i != j )
						cross = ( ( j - i + 3 ) % 3 == 1 ? -1.0f : 1.0f ) * ax[ e ];
					t.fracRot[ j * 3 + i ] = ( i == j ? c : 0.0f ) + ( 1 - c ) * ax[ i ] * ax[ j ] + sn * cross;
				}
		}
		t.veins     = values[ P_NEON_VEINS ] * m;
		t.veinWidth = values[ P_VEIN_WIDTH ];
		Colour( P_VEIN_COL_H, t.veinColor );
		t.contours   = values[ P_SMOKE_CONTOURS ] * m;
		if( Opt( P_CONTOUR_STYLE ) == 0 ) // Lines: drawn on the fog by the post pass, not in the volume
			t.lines = t.contours, t.contours = 0.0f;
		t.lineWidth  = values[ P_LINE_WIDTH ];
		t.lineGlow   = values[ P_LINE_GLOW ];
		t.bands      = values[ P_CONTOUR_BANDS ];
		t.bandSharp  = values[ P_BAND_SHARP ];
		t.bandScroll = float( clk.band );
		t.black      = values[ P_BLACKLIGHT ] * m;
		t.blackBy    = Opt( P_BLACKLIGHT_BY );
		Colour( P_BL_COL_H, t.blackColor );
		for( int k = 0; k < 3; ++k )
			t.veinColor[ k ] *= Alpha( P_VEIN_COL_H ), t.blackColor[ k ] *= Alpha( P_BL_COL_H );
		// Hue Echo: persistence per 60 fps frame, made frame-rate independent
		const float e = std::min( values[ P_HUE_ECHO ], 0.985f ) * std::min( m, 1.0f );
		t.echo        = e > 0.0f ? std::pow( e, std::max( dt, 1e-3f ) * 60.0f ) : 0.0f;
		t.echoHue     = values[ P_ECHO_HUE_STEP ] * m * dt * 60.0f;
		t.medium      = t.palMix > 0.0f || t.film > 0.0f || t.kaleido > 1 || t.fractal > 0.0f || t.veins > 0.0f || t.contours > 0.0f || t.black > 0.0f;
		t.post        = t.prism > 0.0f || t.echo > 0.0f || t.lines > 0.0f;
	}

	// Sim side: whirlpool / hyperspace forces, vortex rings (button or beat), ghost faces (a copy of the mask painted in smoke
	// while it peels off: Ghost Offset away along the mask's front, growing to Ghost Scale, over Ghost Fade Time seconds)
	void BuildSimTrip( FrameState& fs, const Mat4& anchor, float dt )
	{
		SimState& s   = fs.sim;
		const float m = values[ P_TRIP_MASTER ], unit = s.unit;
		s.warp        = TimeWarp();
		s.stepScale   = 1.0f;
		if( LOADEHR_WOW && values[ P_FIRE ] > 0.0f ) // W28 Fire Speed: the burning sim's pace
			s.warp *= values[ P_FIRE_SPEED ], s.stepScale = std::clamp( values[ P_FIRE_SPEED ], 0.1f, 1.0f );
		const float o[ 3 ] = { 0, 0, 0 }, uy[ 3 ] = { 0, 1, 0 };
		anchor.Point( o, s.maskCenter );
		anchor.Dir( uy, s.maskUp );
		Mat4::Norm( s.maskUp );
		s.whirl = values[ P_WHIRLPOOL ] * 1.5f * unit * m;
		s.hyper = values[ P_HYPERSPACE ] * 1.5f * unit * m;
		for( int k = 0; k < 3; ++k )
			s.hyperAxis[ k ] = s.maskCenter[ k ] - fs.camPos[ k ];
		Mat4::Norm( s.hyperAxis );
		// rings (W25): a fired ring keeps its world centre and axis and travels from there for kRingLife s, driven all the
		// way (the sim alone let the core die and smear into a band); the newest two are alive
		const int every = Opt( P_RING_ON_BEAT ) == 0 ? 0 : 1 << ( Opt( P_RING_ON_BEAT ) - 1 );
		if( every > 0 && beatCount != lastRingBeat && beatCount % every == 0 )
			lastRingBeat = beatCount, ringFire = true;
		if( ringFire && m > 0.0f )
		{
			rings[ 1 ] = rings[ 0 ];
			Ring& r = rings[ 0 ];
			r.t     = 0.0f;
			std::memcpy( r.dir, s.eyeDir, sizeof( r.dir ) );
			const int from = Opt( P_RING_FROM );
			if( from == 0 ) // one ring between the eyes (two would merge on the grid), just in front of them
				for( int k = 0; k < 3; ++k )
					r.pos[ k ] = 0.5f * ( s.eyePos[ k ] + s.eyePos[ 3 + k ] ) + s.eyeDir[ k ] * 0.12f * unit;
			else
			{
				const float mouth[ 3 ] = { 0.0f, -0.45f, 1.1f }, behind[ 3 ] = { 0.0f, 0.3f, -1.1f };
				anchor.Point( from == 1 ? mouth : behind, r.pos );
				if( from == 2 )
					std::memcpy( r.dir, s.maskUp, sizeof( r.dir ) );
			}
		}
		ringFire = false;
		const float kRingSmoke = 0.1f, kRingLife = 2.5f; // seconds of smoke on the core; seconds a ring is driven
		s.ringSpeed = values[ P_RING_SPEED ] * unit;
		s.ringSpin  = values[ P_RING_SPIN ] * unit;
		s.ringSmoke = values[ P_RING_SMOKE ] * 5.0f;
		s.ringN     = 0;
		for( Ring& r : rings )
		{
			if( r.t >= kRingLife || m <= 0.0f )
				continue;
			const int i       = s.ringN++;
			const float mk    = std::min( m, 2.0f );
			const float tail  = 1.0f - SmoothStep( 0.6f * kRingLife, kRingLife, r.t );
			s.ringK[ i ]      = r.t < kRingSmoke ? mk : 0.35f * mk * tail;
			s.ringS[ i ]      = r.t < kRingSmoke ? mk : 0.0f;
			s.ringH[ i ]      = 1.0f - 3.0f * SmoothStep( 0.6f * kRingLife, kRingLife, r.t ); // keeps its smoke, then dissolves
			s.ringR[ i ]      = values[ P_RING_SIZE ] * unit * ( 1.0f + 0.12f * r.t );
			const float trav  = 0.6f * s.ringSpeed * r.t / ( 1.0f + 0.35f * r.t ); // matches the drive's 0.6 x speed, slowing
			for( int k = 0; k < 3; ++k )
				s.ringPos[ 3 * i + k ] = r.pos[ k ] + r.dir[ k ] * trav, s.ringDir[ 3 * i + k ] = r.dir[ k ];
		}
		for( Ring& r : rings )
			r.t = std::min( r.t + dt * std::max( s.warp, 0.0f ), 10.0f );
		// ghosts: the spawn phase wraps at 1 (one ghost per wrap); up to 3 are painted at once
		const float rate = values[ P_SMOKE_GHOSTS ], life = std::max( 0.1f, values[ P_GHOST_LIFE ] );
		for( float& a : ghostAge )
			if( a >= 0.0f && ( a += dt * std::max( s.warp, 0.0f ) ) >= life )
				a = -1.0f;
		if( rate > 0.0f && m > 0.0f && s.warp > 0.0f )
		{
			clk.ghost += dt * rate * s.warp;
			if( clk.ghost >= 1.0 )
			{
				clk.ghost = Wrap( clk.ghost, 1.0 );
				for( float& a : ghostAge )
					if( a < 0.0f )
					{
						a = 0.0f;
						break;
					}
			}
		}
		else
			clk.ghost = 0.0;
		s.ghostN     = 0;
		s.ghostSmoke = 25.0f * m;
		Mat4 M;
		std::memcpy( M.m, s.model, sizeof( M.m ) );
		const float toWorld = std::sqrt( M.m[ 0 ] * M.m[ 0 ] + M.m[ 1 ] * M.m[ 1 ] + M.m[ 2 ] * M.m[ 2 ] );
		for( float a : ghostAge )
		{
			if( a < 0.0f || s.ghostN >= 3 )
				continue;
			const float u   = SmoothStep( 0.0f, 1.0f, a / life );
			const float sc  = 1.0f + ( values[ P_GHOST_SCALE ] - 1.0f ) * u;
			const float off = values[ P_GHOST_OFFSET ] * unit * u;
			const float* c  = s.maskCenter;
			const Mat4 G    = Mat4::Translate( c[ 0 ] + s.eyeDir[ 0 ] * off, c[ 1 ] + s.eyeDir[ 1 ] * off, c[ 2 ] + s.eyeDir[ 2 ] * off ) * Mat4::Scale( sc ) *
			               Mat4::Translate( -c[ 0 ], -c[ 1 ], -c[ 2 ] ) * M;
			const Mat4 inv = G.Inverse();
			std::memcpy( s.ghostInv[ s.ghostN ], inv.m, sizeof( inv.m ) );
			s.ghostK[ s.ghostN ]       = 1.0f - a / life;
			s.ghostToWorld[ s.ghostN ] = toWorld * sc;
			++s.ghostN;
		}
		// the TRIP sim passes (age, swirl and vein fields, the forces above) run while any Trip look is on
		s.trip = m > 0.0f && ( fs.trip.medium || fs.trip.post || s.whirl != 0.0f || s.hyper != 0.0f || every > 0 || s.ringN > 0 || rate > 0.0f || s.ghostN > 0 );
	}

	// ---- W26: the dominant model's SDF on the CPU (lightning crawls on it), the same job the sim uploads; null until built
	const SdfField* WowSdf()
	{
		static const std::shared_ptr< GpuModel > none;
		const std::shared_ptr< GpuModel >& cur = domSlot >= 0 ? slots[ domSlot ].current : none;
		if( !cur || !cur->geometry )
			return nullptr;
		if( wowSdfFor != cur->geometry.get() )
		{
			wowSdfFor = cur->geometry.get();
			wowSdf    = RequestSdf( cur->geometry );
		}
		return wowSdf && wowSdf->done.load() && !wowSdf->field.rg.empty() ? &wowSdf->field : nullptr;
	}
	// ---- W26 Stage 5: lightning. A strike makes Bolt Count bolts from its own seed (the strike counter: discrete, so W14-safe),
	// once, by midpoint displacement with branches; Crawl bolts live in the model's raw space (snapped to the SDF's surface, a
	// hair out of it) and follow the mask; the others are world paths. Up to 4 strikes overlap. Each flickers over Bolt Life.
	struct Strike
	{
		float age = 1e9f, life = 0.4f;
		uint32_t seed = 0;
		bool raw = false;              // segments in raw model units (Crawl)
		int slot = 0;                  // LoadEhr W1: the model slot they were struck on (raw segments follow its matrix)
		std::vector< float > segs;     // 8 per segment: a xyz + brightness, b xyz + width factor
		float center[ 3 ] = {};        // raw or world, as the segments
	};
	struct Rng
	{
		uint32_t s;
		float U() { s = s * 1664525u + 1013904223u; return float( ( s >> 8 ) & 0xffffff ) / float( 0xffffff ); }
		float S() { return U() * 2.0f - 1.0f; }
	};
	// a jagged path a -> b (subdivided `levels` times), its branches, into segs; snap (raw space) keeps points on the surface
	void BoltPath( std::vector< float >& segs, const float* a, const float* b, int levels, float bright, float width, Rng& r, const SdfField* snap, float out, int depth )
	{
		std::vector< std::array< float, 3 > > pts = { { a[ 0 ], a[ 1 ], a[ 2 ] }, { b[ 0 ], b[ 1 ], b[ 2 ] } };
		const float jag = 0.06f + 0.5f * values[ P_JAGGEDNESS ], branch = values[ P_BRANCHING ];
		for( int l = 0; l < levels; ++l )
		{
			std::vector< std::array< float, 3 > > next;
			for( size_t i = 0; i + 1 < pts.size(); ++i )
			{
				const auto &p = pts[ i ], &q = pts[ i + 1 ];
				float d[ 3 ] = { q[ 0 ] - p[ 0 ], q[ 1 ] - p[ 1 ], q[ 2 ] - p[ 2 ] };
				const float len = std::sqrt( d[ 0 ] * d[ 0 ] + d[ 1 ] * d[ 1 ] + d[ 2 ] * d[ 2 ] );
				float o[ 3 ]    = { r.S(), r.S(), r.S() };
				const float od  = ( o[ 0 ] * d[ 0 ] + o[ 1 ] * d[ 1 ] + o[ 2 ] * d[ 2 ] ) / std::max( len * len, 1e-12f );
				std::array< float, 3 > m;
				for( int k = 0; k < 3; ++k )
					m[ k ] = 0.5f * ( p[ k ] + q[ k ] ) + ( o[ k ] - od * d[ k ] ) * jag * len * 0.5f;
				if( snap )
					Snap( *snap, m.data(), out );
				next.push_back( p );
				next.push_back( m );
				// a branch from this midpoint: onward roughly the same way, shorter and dimmer
				if( depth < 2 && l < levels - 2 && r.U() < branch * 0.3f )
				{
					std::array< float, 3 > e;
					for( int k = 0; k < 3; ++k )
						e[ k ] = m[ k ] + ( d[ k ] * ( 0.6f + 0.8f * r.U() ) + r.S() * len * 0.9f ) * 0.7f;
					if( snap )
						Snap( *snap, e.data(), out );
					BoltPath( segs, m.data(), e.data(), std::max( 2, levels - l - 2 ), bright * 0.45f, width * 0.6f, r, snap, out, depth + 1 );
				}
			}
			next.push_back( pts.back() );
			pts.swap( next );
		}
		for( size_t i = 0; i + 1 < pts.size(); ++i )
			segs.insert( segs.end(), { pts[ i ][ 0 ], pts[ i ][ 1 ], pts[ i ][ 2 ], bright, pts[ i + 1 ][ 0 ], pts[ i + 1 ][ 1 ], pts[ i + 1 ][ 2 ], width } );
	}
	// onto the SDF's zero set (two Newton steps along the gradient), then `out` raw units outward
	static void Snap( const SdfField& f, float* p, float out )
	{
		const float h = f.boxSize / kSdfRes;
		for( int it = 0; it < 3; ++it )
		{
			float g[ 3 ];
			for( int k = 0; k < 3; ++k )
			{
				float a[ 3 ] = { p[ 0 ], p[ 1 ], p[ 2 ] }, b[ 3 ] = { p[ 0 ], p[ 1 ], p[ 2 ] };
				a[ k ] += h, b[ k ] -= h;
				g[ k ] = ( SdfAt( f, a ) - SdfAt( f, b ) ) / ( 2.0f * h );
			}
			const float gl = std::sqrt( g[ 0 ] * g[ 0 ] + g[ 1 ] * g[ 1 ] + g[ 2 ] * g[ 2 ] );
			if( gl < 1e-6f )
				return;
			const float d = SdfAt( f, p ) - ( it == 2 ? out : 0.0f );
			for( int k = 0; k < 3; ++k )
				p[ k ] -= g[ k ] / gl * d;
		}
	}
	// a random point on the mesh (raw) that faces the camera; tries a few
	bool SurfacePoint( const CpuGeometry& g, const Mat4& M, const float* cam, Rng& r, float* out, float* outN, bool lowest = false )
	{
		const size_t tris = g.idx.size() / 3;
		if( tris == 0 )
			return false;
		bool found = false;
		float bestY = 1e30f;
		for( int t = 0; t < ( lowest ? 48 : 16 ); ++t )
		{
			const size_t tri = std::min( tris - 1, size_t( r.U() * float( tris ) ) );
			const float *a = &g.pos[ g.idx[ tri * 3 ] * 3 ], *b = &g.pos[ g.idx[ tri * 3 + 1 ] * 3 ], *c = &g.pos[ g.idx[ tri * 3 + 2 ] * 3 ];
			float u = r.U(), v = r.U();
			if( u + v > 1.0f )
				u = 1.0f - u, v = 1.0f - v;
			float p[ 3 ], e1[ 3 ], e2[ 3 ], n[ 3 ], wp[ 3 ], wn[ 3 ];
			for( int k = 0; k < 3; ++k )
				p[ k ] = a[ k ] + ( b[ k ] - a[ k ] ) * u + ( c[ k ] - a[ k ] ) * v, e1[ k ] = b[ k ] - a[ k ], e2[ k ] = c[ k ] - a[ k ];
			Mat4::Cross( e1, e2, n );
			M.Point( p, wp );
			M.Dir( n, wn );
			Mat4::Norm( wn );
			float tc[ 3 ] = { cam[ 0 ] - wp[ 0 ], cam[ 1 ] - wp[ 1 ], cam[ 2 ] - wp[ 2 ] };
			Mat4::Norm( tc );
			if( wn[ 0 ] * tc[ 0 ] + wn[ 1 ] * tc[ 1 ] + wn[ 2 ] * tc[ 2 ] < ( t < 10 ? 0.35f : -1.0f ) )
				continue; // facing the camera (after 10 tries: anything, a shell's inside included)
			if( lowest ? wp[ 1 ] < bestY : !found )
			{
				std::memcpy( out, p, sizeof( p ) );
				if( outN )
					std::memcpy( outN, wn, sizeof( wn ) );
				bestY = wp[ 1 ], found = true;
				if( !lowest )
					break;
			}
		}
		return found;
	}
	void MakeStrike( FrameState& fs, const Mat4& anchor, Strike& st )
	{
		st.segs.clear();
		st.age  = 0.0f;
		st.life = values[ P_BOLT_LIFE ];
		st.seed = ++strikeCount;
		Rng r{ st.seed * 2654435761u + 12345u };
		// LoadEhr W1: the strike lands on the dominant model of the chain and stays on it (raw bolts follow that slot's matrix)
		st.slot = std::max( domSlot, 0 );
		const std::shared_ptr< GpuModel >& cur = slots[ st.slot ].current;
		const CpuGeometry* g = cur && cur->geometry ? cur->geometry.get() : nullptr;
		const SdfField* sdf  = WowSdf();
		Mat4 M               = domM;
		const Mat4 inv = M.Inverse();
		const float toRaw = std::sqrt( inv.m[ 0 ] * inv.m[ 0 ] + inv.m[ 1 ] * inv.m[ 1 ] + inv.m[ 2 ] * inv.m[ 2 ] );
		float u1[ 3 ] = { 1, 0, 0 }, uw[ 3 ];
		anchor.Dir( u1, uw );
		const float unit = std::sqrt( uw[ 0 ] * uw[ 0 ] + uw[ 1 ] * uw[ 1 ] + uw[ 2 ] * uw[ 2 ] ); // world length of a mask unit
		const int count  = std::clamp( (int)std::lround( values[ P_BOLT_COUNT ] ), 1, 8 );
		int path         = Opt( P_BOLT_PATH );
		if( path == 3 && fs.inst.size() < 2 )
			path = 0; // no clones: crawl
		st.raw = path == 0;
		float csum[ 3 ] = {};
		int cn          = 0;
		for( int i = 0; i < count; ++i )
		{
			float a[ 3 ], b[ 3 ], n[ 3 ];
			if( path == 0 )
			{
				if( !g || !SurfacePoint( *g, M, fs.camPos, r, a, n ) )
					continue;
				// the far end: another point, between 0.3 and 1.3 mask units away (raw)
				bool ok = false;
				for( int t = 0; t < 12 && !ok; ++t )
				{
					if( !SurfacePoint( *g, M, fs.camPos, r, b, nullptr ) )
						break;
					const float d = std::sqrt( ( b[ 0 ] - a[ 0 ] ) * ( b[ 0 ] - a[ 0 ] ) + ( b[ 1 ] - a[ 1 ] ) * ( b[ 1 ] - a[ 1 ] ) + ( b[ 2 ] - a[ 2 ] ) * ( b[ 2 ] - a[ 2 ] ) ) / toRaw / unit;
					ok = d > 0.6f && d < 1.6f;
				}
				if( !ok )
					continue;
				const float out = 0.012f * unit * toRaw;
				if( sdf )
					Snap( *sdf, a, out ), Snap( *sdf, b, out );
				BoltPath( st.segs, a, b, 6, 1.0f, 1.0f, r, sdf, out, 0 );
			}
			else
			{
				if( path == 1 )
				{
					// eye to eye (Light C's openings), a little in front of them
					float l[ 3 ], rr[ 3 ];
					EyeOpen( 0, l ), EyeOpen( 1, rr );
					anchor.Point( l, a ), anchor.Point( rr, b );
					// out of the surface along the mask's front (an eye opening's rim is inside the SDF's shell)
					float fwd[ 3 ], fz[ 3 ] = { 0, 0, 1 };
					anchor.Dir( fz, fwd );
					Mat4::Norm( fwd );
					for( float* q : { a, b } )
						for( int it = 0; it < 40 && sdf; ++it )
						{
							float rq[ 3 ];
							inv.Point( q, rq );
							if( SdfAt( *sdf, rq ) / toRaw > 0.04f * unit )
								break;
							for( int k = 0; k < 3; ++k )
								q[ k ] += fwd[ k ] * 0.025f * unit;
						}
				}
				else
				{
					float raw[ 3 ];
					if( !g || !SurfacePoint( *g, M, fs.camPos, r, raw, n, path == 2 ) )
						continue;
					M.Point( raw, a );
					if( path == 2 )
					{
						const float floorY = values[ P_POS_Y ] + values[ P_FLOOR_HEIGHT ];
						b[ 0 ] = a[ 0 ] + r.S() * 0.6f * unit, b[ 1 ] = floorY, b[ 2 ] = a[ 2 ] + r.S() * 0.6f * unit;
					}
					else
					{
						// a clone (any copy but the first, the original when kept): toward its centre
						const Instance& in = fs.inst[ 1 + size_t( r.U() * float( fs.inst.size() - 1 ) ) % ( fs.inst.size() - 1 ) ];
						Mat4 C;
						std::memcpy( C.m, in.m, sizeof( C.m ) );
						C.Point( g->fitCenter, b );
					}
				}
				if( path == 1 )
				{
					// eye to eye: arc forward over the nose (a first midpoint pulled out of the face)
					float fwd[ 3 ], fz[ 3 ] = { 0, 0, 1 }, m[ 3 ];
					anchor.Dir( fz, fwd );
					Mat4::Norm( fwd );
					const float span = std::sqrt( ( b[ 0 ] - a[ 0 ] ) * ( b[ 0 ] - a[ 0 ] ) + ( b[ 1 ] - a[ 1 ] ) * ( b[ 1 ] - a[ 1 ] ) + ( b[ 2 ] - a[ 2 ] ) * ( b[ 2 ] - a[ 2 ] ) );
					for( int k = 0; k < 3; ++k )
						m[ k ] = 0.5f * ( a[ k ] + b[ k ] ) + fwd[ k ] * 0.35f * span;
					BoltPath( st.segs, a, m, 6, 1.0f, 1.0f, r, nullptr, 0.0f, 0 );
					BoltPath( st.segs, m, b, 6, 1.0f, 1.0f, r, nullptr, 0.0f, 0 );
				}
				else
					BoltPath( st.segs, a, b, 7, 1.0f, 1.0f, r, nullptr, 0.0f, 0 );
			}
			for( int k = 0; k < 3; ++k )
				csum[ k ] += 0.5f * ( a[ k ] + b[ k ] );
			++cn;
		}
		for( int k = 0; k < 3; ++k )
			st.center[ k ] = cn ? csum[ k ] / cn : 0.0f;
		if( LogVerbose() )
			Log( "STRIKE %u: path %d, %d bolts, %zu segments", st.seed, path, cn, st.segs.size() / 8 );
	}
	void BuildLightning( FrameState& fs, const Mat4& anchor, float dt )
	{
		const int every = Opt( P_STRIKE_BEAT ) == 0 ? 0 : 1 << ( Opt( P_STRIKE_BEAT ) - 1 );
		bool fire       = strikeRequest;
		strikeRequest   = false;
		if( every > 0 && beatCount != lastStrikeBeat && beatCount % every == 0 )
			lastStrikeBeat = beatCount, fire = true;
		// Strike Rate: a strike each time the phase passes a whole number (jittered by the strike's own hash)
		const float rate = values[ P_STRIKE_RATE ];
		if( rate > 0.0f )
		{
			clk.strike += dt * rate * ( 0.5 + double( ( ( strikeCount * 2654435761u ) >> 8 ) & 0xffff ) / 65535.0 );
			if( clk.strike >= 1.0 )
				clk.strike = Wrap( clk.strike, 1.0 ), fire = true;
		}
		BoltState& b = fs.bolts;
		b.segs.clear();
		if( fire && values[ P_BOLT_COL_A ] > 0.0f )
		{
			strikes[ strikeNext ] = Strike();
			MakeStrike( fs, anchor, strikes[ strikeNext ] );
			strikeNext = ( strikeNext + 1 ) % 4;
		}
		float flash = 0, fc[ 3 ] = {}, fw = 0;
		for( Strike& st : strikes )
		{
			if( st.age >= st.life || st.segs.empty() )
				continue;
			const Mat4& M = slotM[ std::clamp( st.slot, 0, kModelSlots - 1 ) ]; // raw bolts ride the model they were struck on
			// flicker: a few re-strikes inside the decay (hash of the strike and the 1/30 s step)
			const float a   = st.age / std::max( st.life, 1e-3f );
			const uint32_t h = ( st.seed * 2246822519u ) ^ ( uint32_t( st.age * 30.0f ) * 3266489917u );
			const float env = std::exp( -3.0f * a ) * ( ( ( h >> 7 ) & 255 ) > 70 ? 1.0f : 0.35f ) * ( 1.0f - SmoothStep( 0.8f, 1.0f, a ) );
			for( size_t i = 0; i < st.segs.size(); i += 8 )
			{
				float p[ 8 ];
				std::memcpy( p, &st.segs[ i ], sizeof( p ) );
				if( st.raw )
					M.Point( &st.segs[ i ], p ), M.Point( &st.segs[ i + 4 ], p + 4 ), p[ 3 ] = st.segs[ i + 3 ], p[ 7 ] = st.segs[ i + 7 ];
				p[ 3 ] *= env;
				b.segs.insert( b.segs.end(), p, p + 8 );
			}
			float c[ 3 ];
			if( st.raw )
				M.Point( st.center, c );
			else
				std::memcpy( c, st.center, sizeof( c ) );
			for( int k = 0; k < 3; ++k )
				fc[ k ] += c[ k ] * env;
			fw += env, flash = std::max( flash, env );
			st.age += dt;
		}
		Colour( P_BOLT_COL_H, b.color );
		for( float& c : b.color )
			c *= values[ P_BOLT_COL_A ] * 1.5f;
		b.width = values[ P_BOLT_WIDTH ];
		{
			float u1[ 3 ] = { 1, 0, 0 }, uw[ 3 ];
			anchor.Dir( u1, uw );
			b.tol = 0.06f * std::sqrt( uw[ 0 ] * uw[ 0 ] + uw[ 1 ] * uw[ 1 ] + uw[ 2 ] * uw[ 2 ] ); // 6 % of a mask unit
		}
		b.glow  = values[ P_BOLT_GLOW ];
		// the flash: a point light at the bolts' centre (surface) and the smoke's ambient light
		const float fl = flash * values[ P_BOLT_FLASH ];
		if( fl > 0.0f && fw > 0.0f )
		{
			float wc[ 3 ] = { 0.75f + 0.25f * b.color[ 0 ], 0.75f + 0.25f * b.color[ 1 ], 0.75f + 0.25f * b.color[ 2 ] };
			for( int k = 0; k < 3; ++k )
				fs.vol.flash[ k ] = wc[ k ] * fl * 0.08f;
			if( values[ P_SMOKE_AMOUNT ] > 0.0f )
				fs.vol.active = true;
			if( fs.xlCount < kMaxXLights )
			{
				XLight& x = fs.xl[ fs.xlCount++ ];
				x.type = 1, x.shadow = false;
				for( int k = 0; k < 3; ++k )
					x.pos[ k ] = fc[ k ] / fw, x.color[ k ] = wc[ k ] * fl * 2.5f;
			}
		}
	}

	// ---- W26 Stage 8: caustics. The caustic light sits 5 mask units out along the key's direction, aimed at the mask, its
	// frustum wide enough for the mask and the floor under it. The ripple drifts in 64-periodic noise (wraps at 64).
	void BuildCaustics( FrameState& fs, const Mat4& anchor, float dt )
	{
		CausticState& c = fs.caustic;
		const float k   = values[ P_CAUSTICS ] * Alpha( P_CAUSTIC_COL_H );
		c.on = k > 0.0f && ( values[ P_CAUSTIC_FLOOR ] > 0.0f || values[ P_CAUSTIC_SMOKE ] > 0.0f || values[ P_CAUSTIC_SURFACE ] > 0.0f );
		clk.caustic = Wrap( clk.caustic + dt * 0.5, 64.0 );
		if( !c.on )
			return;
		float center[ 3 ], ux[ 3 ] = { 1, 0, 0 }, uw[ 3 ];
		const float o[ 3 ] = { 0, 0, 0 };
		anchor.Point( o, center );
		anchor.Dir( ux, uw );
		const float unit = std::max( 1e-3f, std::sqrt( uw[ 0 ] * uw[ 0 ] + uw[ 1 ] * uw[ 1 ] + uw[ 2 ] * uw[ 2 ] ) );
		const float D    = 5.0f * unit;
		for( int i = 0; i < 3; ++i )
			c.pos[ i ] = center[ i ] + fs.keyDir[ i ] * D, c.dir[ i ] = -fs.keyDir[ i ];
		c.fov       = 2.0f * std::atan( 2.6f / 5.0f ) / kDeg;
		c.gain      = 1.0f + D * D;
		c.wholeMask = Opt( P_CAUSTIC_SOURCE ) == 1;
		c.ior       = values[ P_CAUSTIC_IOR ];
		c.rainbow   = values[ P_CAUSTIC_RAINBOW ];
		c.sharp     = values[ P_CAUSTIC_SHARP ];
		c.ripple    = values[ P_CAUSTIC_RIPPLE ];
		c.phase     = float( clk.caustic );
		c.floor     = values[ P_CAUSTIC_FLOOR ];
		c.smoke     = values[ P_CAUSTIC_SMOKE ];
		c.surface   = values[ P_CAUSTIC_SURFACE ];
		Colour( P_CAUSTIC_COL_H, c.color );
		for( float& v : c.color )
			v *= k;
		if( c.smoke > 0.0f && !fs.vol.active )
			fs.vol.active = fs.vol.projOnly = true; // the light in the smoke lives in the fog's march target
	}

	// ---- W26 Stage 7: lava cracks. Crack Start runs the front over the mask in Crack Time from a new seed (a camera-facing
	// surface point, picked by the start's own count: discrete); Reverse heals it back; the Crack Spread slider scrubs by hand
	// until the next press (as Dust). The lava flow drifts at 0.12 / s in 64-periodic noise (wraps at 64).
	// W28 (Amrit: "lava comes and goes"): cracks that are out STAY until Crack Reverse. A clip trigger spreads them from nothing
	// (OnTrigger, once the model is in; the seed the first Crack Start would pick); Crack On Beat cracks on its first beat and
	// then makes the lava flare on the beat (W26 healed it on every other beat).
	void BuildLava( FrameState& fs, const Mat4& anchor, float dt )
	{
		LavaState& l   = fs.lava;
		const int every = Opt( P_CRACK_BEAT ) == 0 ? 0 : 1 << ( Opt( P_CRACK_BEAT ) - 1 );
		if( every > 0 && beatCount != lastCrackBeat && beatCount % every == 0 )
		{
			lastCrackBeat = beatCount;
			if( crackTarget < 0.5f )
				crackPress = 1;
			else
				flareT = 0.0f;
		}
		flareT = std::min( flareT + dt, 10.0f );
		if( crackAuto && domSlot >= 0 && slots[ domSlot ].current && slots[ domSlot ].current->geometry )
		{
			crackAuto = false;
			if( values[ P_LAVA ] > 0.0f && every == 0 && crackPress == 0 )
				crackPress = 3;
		}
		if( crackPress == 1 || crackPress == 3 )
		{
			crackProgress = 0.0f, crackTarget = 1.0f, crackManual = false;
			// a new seed: a surface point facing the camera (fitted space) on the chain's dominant model
			static const std::shared_ptr< GpuModel > none;
			const std::shared_ptr< GpuModel >& cur = domSlot >= 0 ? slots[ domSlot ].current : none;
			if( cur && cur->geometry )
			{
				const Mat4 M = domM;
				Rng r{ ( crackPress == 3 ? crackStarts + 1 : ++crackStarts ) * 2246822519u + 777u };
				float raw[ 3 ];
				if( SurfacePoint( *cur->geometry, M, fs.camPos, r, raw, nullptr ) )
					for( int k = 0; k < 3; ++k )
						crackSeed[ k ] = ( raw[ k ] - cur->fitCenter[ k ] ) * cur->fitScale;
			}
		}
		else if( crackPress == 2 )
		{
			if( crackManual )
				crackProgress = values[ P_CRACK_SPREAD ];
			crackTarget = 0.0f, crackManual = false;
		}
		crackPress = 0;
		const float dur = every > 0 ? std::min( values[ P_CRACK_TIME ], 0.9f * every * std::max( 0.1f, beatPeriod ) ) : values[ P_CRACK_TIME ];
		crackProgress   = crackTarget > crackProgress ? std::min( crackTarget, crackProgress + dt / dur ) : std::max( crackTarget, crackProgress - dt / dur );
		const float pr  = crackManual ? values[ P_CRACK_SPREAD ] : crackProgress;
		l.on    = values[ P_LAVA ] > 0.0f && pr > 0.0f;
		l.front = SmoothStep( 0.0f, 1.0f, pr ) * 3.6f;
		std::memcpy( l.seed, crackSeed, sizeof( crackSeed ) );
		l.scale = values[ P_CRACK_SCALE ] * 0.5f; // per fitted unit (a mask unit = 2 fitted)
		l.split = l.on ? values[ P_LAVA_SPLIT ] : 0.0f;
		l.glow  = values[ P_LAVA ] * values[ P_LAVA_GLOW ] * ( every > 0 ? 1.0f + 0.8f * std::exp( -flareT * 3.0f ) : 1.0f );
		l.temp  = values[ P_LAVA_TEMP ];
		l.width = values[ P_CRACK_WIDTH ];
		l.tintMix = Alpha( P_LAVA_TINT_H );
		Colour( P_LAVA_TINT_H, l.tint );
		clk.lavaFlow = Wrap( clk.lavaFlow + dt * 0.12, 64.0 );
		l.flow  = float( clk.lavaFlow );
		l.haze  = l.on ? values[ P_HEAT_HAZE ] * values[ P_LAVA ] : 0.0f;
		clk.lavaHaze = Wrap( clk.lavaHaze + dt * 0.5, 1.0 );
		l.hazePhase  = float( clk.lavaHaze );
		if( l.split > 0.0f )
			fs.shape.on = true; // the split moves triangles in the soup stage
		(void)anchor;
	}

	// ---- W26 Stage 6: the firefly swarm. Release (button / beat): one outward kick, the pull home off for Release Time, then it
	// eases back on over 1.5 s. Clocks: the wander drift wraps at 64 (64-periodic noise), the blink at kSwarmLoop s.
	void BuildSwarm( FrameState& fs, const Mat4& anchor, float dt )
	{
		SwarmState& w = fs.swarm;
		static const int kCounts[ 6 ] = { 0, 2000, 5000, 10000, 20000, 40000 };
		w.count = kCounts[ std::clamp( Opt( P_SWARM_COUNT ), 0, 5 ) ] * ( values[ P_FIREFLY_COL_A ] > 0.0f ? 1 : 0 );
		const int every = Opt( P_RELEASE_BEAT ) == 0 ? 0 : 1 << ( Opt( P_RELEASE_BEAT ) - 1 );
		if( every > 0 && beatCount != lastReleaseBeat && beatCount % every == 0 )
			lastReleaseBeat = beatCount, swarmRelease = true;
		w.kick = 0.0f;
		if( swarmRelease && w.count > 0 )
			releaseT = 0.0f, w.kick = 1.0f;
		swarmRelease = false;
		releaseT     = std::min( releaseT + dt, 1000.0f );
		const float hold = every > 0 ? std::min( values[ P_RELEASE_TIME ], 0.6f * every * std::max( 0.1f, beatPeriod ) ) : values[ P_RELEASE_TIME ];
		w.attract    = values[ P_ATTRACTION ] * SmoothStep( hold, hold + 1.0f, releaseT );
		w.flock      = values[ P_FLOCK ];
		w.wander     = values[ P_SWARM_SPEED ];
		w.offset     = values[ P_SWARM_OFFSET ];
		w.size       = values[ P_FIREFLY_SIZE ];
		w.twinkle    = values[ P_TWINKLE ];
		w.dt         = dt;
		clk.swarm    = Wrap( clk.swarm + dt * 0.15 * w.wander, 64.0 );
		clk.swarmBlink = Wrap( clk.swarmBlink + dt, kSwarmLoop );
		w.drift      = float( clk.swarm );
		w.time       = float( clk.swarmBlink );
		float ux[ 3 ] = { 1, 0, 0 }, uw[ 3 ];
		anchor.Dir( ux, uw );
		w.unit = std::max( 1e-3f, std::sqrt( uw[ 0 ] * uw[ 0 ] + uw[ 1 ] * uw[ 1 ] + uw[ 2 ] * uw[ 2 ] ) );
		Colour( P_FIREFLY_COL_H, w.color );
		for( float& c : w.color )
			c *= values[ P_FIREFLY_GLOW ] * Alpha( P_FIREFLY_COL_H ) * 0.5f;
		w.maskVisible = On( P_MASK_VISIBLE );
		if( !w.maskVisible )
			for( PartControls& p : fs.parts )
				p.visible = false;
		w.reset = w.count != lastSwarmCount;
		lastSwarmCount = w.count;
	}

	// ---- W26 Stage 2: the projector. Placed and aimed like Light A; the picture = the FX input or Proj Image (decoded on a
	// worker thread, <= 2048 px, bottom-up rows, CPU mips: glGenerateMipmap blocks the render thread)
	void BuildProjector( FrameState& fs, const Mat4& anchor, const float* eye, float dt )
	{
		(void)eye;
		ProjState& p = fs.projector;
		const float gain = values[ P_PROJ_INTENSITY ] * Alpha( P_PROJ_COL_H );
		p.on = On( P_PROJ_ENABLE ) && gain > 0.0f && ( values[ P_PROJ_BEAM ] > 0.0f || values[ P_PROJ_SURFACE ] > 0.0f );
#if LOADEHR_FX
		p.source = Opt( P_PROJ_SOURCE );
#else
		p.source = 1;
#endif
		const std::string& file = texts[ P_PROJ_FILE ];
		if( p.source == 1 && file != projImageFile )
		{
			projImageFile = file;
			projImageJob.reset();
			projImage.reset();
			video.reset();
			SetVideoTime( "" );
			std::string ext = file.substr( file.find_last_of( '.' ) + 1 );
			std::transform( ext.begin(), ext.end(), ext.begin(), ::tolower );
			if( ext == "mov" || ext == "mp4" || ext == "m4v" )
				video = std::make_shared< VideoPlayer >( file ), videoLogged = false, videoRestart = true; // W2: decoded on its own thread
			else if( !file.empty() )
				projImageJob = DecodeProjImage( file, ++projImageSerial );
		}
		p.video.reset();
		if( video && p.on && p.source == 1 )
			p.video = VideoClock( dt );
		p.videoStraight = Opt( P_VIDEO_ALPHA ) == 1;
		if( projImageJob && projImageJob->done.load() )
		{
			if( projImageJob->image.w > 0 )
				projImage = std::make_shared< const ProjImage >( std::move( projImageJob->image ) );
			Log( "PROJ image %s: %s", projImageFile.c_str(), projImage ? Fmt( "%dx%d, %zu mips", projImage->w, projImage->h, projImage->mips.size() ).c_str() : "could not decode" );
			projImageJob.reset();
		}
		p.image = projImage;
		if( !p.on )
			return;
		float center[ 3 ];
		const float o[ 3 ] = { 0, 0, 0 };
		anchor.Point( o, center );
		const bool anchored = On( P_PROJ_ANCHOR );
		const float lp[ 3 ] = { values[ P_PROJ_POS_X ], values[ P_PROJ_POS_Y ], values[ P_PROJ_POS_Z ] };
		if( anchored )
			anchor.Point( lp, p.pos );
		else
			std::memcpy( p.pos, lp, sizeof( lp ) );
		float d[ 3 ];
		if( Opt( P_PROJ_AIM_MODE ) == 0 )
			for( int k = 0; k < 3; ++k )
				d[ k ] = center[ k ] - p.pos[ k ];
		else
		{
			const float yaw = values[ P_PROJ_AIM_YAW ] * kDeg, pitch = values[ P_PROJ_AIM_PITCH ] * kDeg;
			const float local[ 3 ] = { std::cos( pitch ) * std::sin( yaw ), std::sin( pitch ), -std::cos( pitch ) * std::cos( yaw ) }; // 0, 0 = toward -Z (from the camera side into the scene)
			if( anchored )
				anchor.Dir( local, d );
			else
				std::memcpy( d, local, sizeof( local ) );
		}
		if( d[ 0 ] * d[ 0 ] + d[ 1 ] * d[ 1 ] + d[ 2 ] * d[ 2 ] < 1e-10f )
			d[ 2 ] = -1.0f;
		Mat4::Norm( d );
		std::memcpy( p.dir, d, sizeof( d ) );
		p.fovX = values[ P_PROJ_THROW ];
		p.focus = values[ P_PROJ_FOCUS ];
		p.beam = values[ P_PROJ_BEAM ];
		p.surface = values[ P_PROJ_SURFACE ];
		float c[ 3 ];
		Colour( P_PROJ_COL_H, c );
		for( int k = 0; k < 3; ++k )
			p.color[ k ] = std::max( 0.0f, c[ k ] ) * gain;
		if( p.beam > 0.0f && !fs.vol.active )
			fs.vol.active = fs.vol.projOnly = true; // the beam lives in the fog's march target
	}
	// W5: the wireframe. Over Surface with every W5 dial at rest = the W1 wire on the plain programs (fs.wireMix); anything else
	// draws through the WIRE twins (fs.wire.on). Wireframe Mix = the lines' strength in every mode (0 = no lines).
	void BuildWire( FrameState& fs ) const
	{
		FrameState::WireState& w = fs.wire;
		w.mode      = Opt( P_WIRE_MODE );
		w.back      = Opt( P_WIRE_BACK );
		w.edges     = Opt( P_WIRE_EDGES );
		w.mix       = values[ P_WIRE_MIX ];
		Colour( P_WIRE_COL_H, w.color );
		w.colorMix  = Alpha( P_WIRE_COL_H );
		w.glow      = values[ P_WIRE_GLOW ];
		w.bloom     = values[ P_WIRE_BLOOM ];
		w.bloomSize = values[ P_WIRE_BLOOM_SIZE ];
		w.fade      = values[ P_WIRE_FADE ];
		const bool w5 = w.mode >= 2 || w.colorMix > 0.0f || w.glow > 0.0f || w.bloom > 0.0f || w.fade > 0.0f || w.back == 1 || w.edges == 1;
		w.on             = w.mode >= 1 && w5;
		fs.wireMix       = w.mode == 1 && !w5 ? w.mix : 0.0f;
		fs.wireThickness = values[ P_WIRE_THICKNESS ];
	}
	// W2: the video's play position. Loop: wraps at the clip length; Ping-Pong: a clock wrapped at twice the length, mirrored back;
	// Once: runs to the end and holds the last frame. Video Speed x real time; Video Restart / a clip trigger: back to the start.
	// W4: Video Speed < 0 plays backwards (Loop / Ping-Pong wrap the same; Once runs back to frame 0 and holds it; a restart of Once
	// at a negative speed starts from the end). Scrub: no clock, the frame = round(Video Position x (frames - 1)). A user change
	// of Video Position in the other modes cues (the clock jumps to that frame and plays on); the plugin pushes its own position
	// into Video Position ~15 times a second (the playhead), and Video Time a few times a second.
	std::shared_ptr< const VideoFrame > VideoClock( float dt )
	{
		if( video->Failed() )
		{
			if( !videoLogged )
				Log( "PROJ video %s: %s", projImageFile.c_str(), video->Info().c_str() ), videoLogged = true;
			SetVideoTime( "video failed: " + video->Info() );
			return nullptr;
		}
		if( !video->Ready() )
			return nullptr;
		if( !videoLogged )
			Log( "PROJ video %s: %s", projImageFile.c_str(), video->Info().c_str() ), videoLogged = true;
		const double fd  = video->FrameDuration();
		const int n      = std::max( 1, video->Frames() );
		const double len = std::max( video->Duration(), fd );
		int mode         = std::clamp( Opt( P_VIDEO_LOOP ), 0, 3 );
		// W6 (Amrit: Scrub "Limited."): Scrub needs a codec whose every frame decodes on its own (DXV, HAP); any other plays it as Loop
		videoScrubRefused = mode == 3 && !video->Intra();
		if( videoScrubRefused )
			mode = 0;
		const double spd = values[ P_VIDEO_SPEED ];
		auto at          = [ & ]( float p ) { return int( std::lround( std::clamp( p, 0.0f, 1.0f ) * ( n - 1 ) ) ); }; // position -> frame
		if( mode == 3 )
		{
			// SCRUB: the frame is the slider; the clock waits there (switching back to a playing mode plays on from this frame)
			const int i       = at( values[ P_VIDEO_POS ] );
			const bool fwd    = i >= videoScrubLast;
			videoScrubLast    = i;
			clk.video         = ( i + 0.5 ) * fd;
			videoCue          = -1.0f, videoRestart = false;
			auto f            = video->Frame( ( i + 0.5 ) * fd, fwd, false, 1 );
			VideoStats( dt, i, n, fd );
			return f;
		}
		const bool back0 = mode == 1 && clk.video >= len; // Ping-Pong's half the clock is in before this frame
		if( videoCue >= 0.0f )
		{
			const double t = ( at( videoCue ) + 0.5 ) * fd; // the middle of the cued frame
			clk.video      = mode == 1 && back0 ? 2.0 * len - t : t;
			videoCueHold   = at( videoCue ), videoCueWait = 0.0f; // the clock waits on the cued frame until it is on screen
			videoCue       = -1.0f;
		}
		if( videoRestart )
			clk.video = mode == 2 && spd < 0.0 ? len : 0.0, videoRestart = false, videoCueHold = -1; // a trigger after a cue in the same frame: the restart wins
		if( videoCueHold >= 0 && ( videoCueWait += dt ) > 1.0f )
			videoCueHold = -1; // not decoded within 1 s: play on regardless
		const double adv = videoCueHold >= 0 ? 0.0 : double( dt ) * spd;
		clk.video        = mode == 0 ? Wrap( clk.video + adv, len ) : mode == 1 ? Wrap( clk.video + adv, 2.0 * len ) : std::clamp( clk.video + adv, 0.0, len );
		const bool back  = mode == 1 && clk.video >= len;
		const double pos = std::min( back ? 2.0 * len - clk.video : clk.video, len - 0.5 * fd );
		// while a cue waits, the last frame stays (as in Scrub: no frame from before the cue is shown; its latency counts as a SCRUB line)
		auto f           = video->Frame( pos, ( spd >= 0.0 ) != back, mode == 0, videoCueHold >= 0 ? 2 : 0 );
		if( f && f->index == videoCueHold )
			videoCueHold = -1;
		const int i      = std::clamp( int( std::floor( pos / fd + 1e-4 ) ), 0, n - 1 );
		videoScrubLast   = i;
		// the playhead: Video Position follows the play position (rate-limited; Arena reads it back and may echo it late, SCOTTY-005)
		videoPushT += dt;
		const float pv = n > 1 ? float( i ) / float( n - 1 ) : 0.0f;
		if( videoPushT >= 1.0f / 15.0f && pv != values[ P_VIDEO_POS ] )
		{
			videoPushT          = 0.0f;
			values[ P_VIDEO_POS ] = pv;
			videoPushed[ videoPushN++ % 8 ] = pv;
			RaiseParamEvent( P_VIDEO_POS, FF_EVENT_FLAG_VALUE );
		}
		VideoStats( dt, i, n, fd );
		return f;
	}
	// a host set equal to one of the last 8 pushed playhead values is Arena writing our own value back, not a user cue
	bool PushEcho( float v ) const
	{
		for( float p : videoPushed )
			if( p >= 0.0f && std::fabs( p - v ) < 1e-6f )
				return true;
		return false;
	}
	void SetVideoTime( const std::string& t )
	{
		if( t == texts[ P_VIDEO_TIME ] )
			return;
		texts[ P_VIDEO_TIME ] = t;
		RaiseParamEvent( P_VIDEO_TIME, FF_EVENT_FLAG_VALUE );
	}
	// Video Time ~5 times a second; Debug Log: the VIDEO line and the scrub latencies once a second
	void VideoStats( float dt, int i, int n, double fd )
	{
		videoTimeT += dt;
		if( videoTimeT >= 0.2f )
		{
			videoTimeT  = 0.0f;
			auto mmss   = []( double s ) {
				const long t = std::lround( s * 10.0 ); // tenths
				return Fmt( "%02ld:%02ld.%ld", t / 600, t / 10 % 60, t % 10 );
			};
			SetVideoTime( ( videoScrubRefused ? "Scrub: DXV / HAP only  " : "" ) + mmss( i * fd ) + " / " + mmss( n * fd ) + Fmt( "  frame %d / %d  ", i + 1, n ) + video->Codec() );
		}
		// Debug Log: one VIDEO line per second (frames shown late = the last decoded frame stood in; the drops are only logged here)
		videoStatT += dt;
		if( LogVerbose() && videoStatT >= 1.0f )
		{
			Log( "VIDEO %s | decoded %llu (%.2f ms each, worker), late %llu (+%llu), upload %.2f ms (render thread%s), cache %.0f MB", FileName( projImageFile ).c_str(),
			     (unsigned long long)video->decoded, video->decoded ? video->decodeMs / video->decoded : 0.0, (unsigned long long)video->late,
			     (unsigned long long)( video->late - videoLateLogged ), renderer.videoUploadMs, values[ P_PROJ_FOCUS ] > 0.0f ? ", + mips" : "", video->cacheBytes / 1048576.0 );
			videoLateLogged = video->late;
		}
		if( videoStatT >= 1.0f )
		{
			videoStatT   = 0.0f;
			uint64_t sup = 0;
			const auto scrubs = video->TakeScrub( &sup ); // drained every second, logged with Debug Log
			if( LogVerbose() )
			{
				for( const auto& st : scrubs )
					Log( "SCRUB %s frame %d: %.2f ms, held %d", FileName( projImageFile ).c_str(), st.frame, st.ms, st.held );
				if( sup )
					Log( "SCRUB %s: %llu targets superseded", FileName( projImageFile ).c_str(), (unsigned long long)sup );
			}
		}
	}
	struct ProjDecode
	{
		std::atomic< bool > done{ false };
		ProjImage image;
	};
	static std::shared_ptr< ProjDecode > DecodeProjImage( const std::string& path, uint64_t serial )
	{
		auto job = std::make_shared< ProjDecode >();
		std::thread( [ job, path, serial ]() {
			int w = 0, h = 0, c = 0;
			stbi_set_flip_vertically_on_load_thread( 1 ); // GL rows: bottom-up, like the host's input
			uint8_t* px = stbi_load( path.c_str(), &w, &h, &c, 4 );
			if( px && w > 0 && h > 0 )
			{
				std::vector< uint8_t > lvl( px, px + size_t( w ) * h * 4 );
				stbi_image_free( px );
				while( std::max( w, h ) > 2048 ) // halve to <= 2048 (box)
					lvl = HalveRGBA( lvl, w, h );
				ProjImage& im = job->image;
				im.w = w, im.h = h, im.serial = serial;
				im.mips.push_back( lvl );
				while( w > 1 || h > 1 )
					im.mips.push_back( lvl = HalveRGBA( lvl, w, h ) );
			}
			job->done.store( true );
		} ).detach();
		return job;
	}
	// one 2x2 box step; w / h become the new size
	static std::vector< uint8_t > HalveRGBA( const std::vector< uint8_t >& src, int& w, int& h )
	{
		const int nw = std::max( 1, w / 2 ), nh = std::max( 1, h / 2 );
		std::vector< uint8_t > dst( size_t( nw ) * nh * 4 );
		for( int y = 0; y < nh; ++y )
			for( int x = 0; x < nw; ++x )
				for( int k = 0; k < 4; ++k )
				{
					int sum = 0;
					for( int dy = 0; dy < 2; ++dy )
						for( int dx = 0; dx < 2; ++dx )
							sum += src[ ( size_t( std::min( h - 1, y * 2 + dy ) ) * w + std::min( w - 1, x * 2 + dx ) ) * 4 + k ];
					dst[ ( size_t( y ) * nw + x ) * 4 + k ] = uint8_t( ( sum + 2 ) / 4 );
				}
		w = nw, h = nh;
		return dst;
	}

	// ---- Light C: two spots behind the eye openings. Pos = the opening; the light sits Behind Depth behind it
	// along its aim, and with Front Only the medium only counts beyond the opening.
	void BuildEyeLights( FrameState& fs, int& n, const Mat4& anchor, const float* eye, float pulse, float dt )
	{
		Drift( 2, values[ P_LC_HUE_DRIFT ], dt ); // one offset for C Left and C Right (one C Hue Drift dial)
		if( !On( P_LC_ENABLE ) )
			return;
		const bool anchored = On( P_LC_ANCHOR ), mirror = On( P_LC_MIRROR );
		const int aim       = Opt( P_LC_AIM_MODE );
		const float behind  = values[ P_LC_BEHIND ];
		const float gain    = values[ P_LC_INTENSITY ] * ( 1.0f + values[ P_LC_BEAT ] * pulse ) * ( 1.0f + values[ P_LC_AUDIO ] * audio[ 0 ] );
		for( int side = 0; side < 2; ++side )
		{
			XLight& x = fs.xl[ n++ ];
			x.type    = 0;
			x.shadow  = true;
			float open[ 3 ];
			EyeOpen( side, open ); // Light C's opening (+ the chain's eye offset)
			// aim (mask space when anchored, world otherwise), then the light behind the opening
			float d[ 3 ];
			if( aim == 1 )
			{
				float w[ 3 ];
				if( anchored )
					anchor.Point( open, w );
				else
					std::memcpy( w, open, sizeof( w ) );
				for( int k = 0; k < 3; ++k )
					d[ k ] = eye[ k ] - w[ k ];
				Mat4::Norm( d );
				float mu[ 3 ] = { 1, 0, 0 }, mw[ 3 ]; // world length of one mask unit
				anchor.Dir( mu, mw );
				const float unit = anchored ? std::sqrt( mw[ 0 ] * mw[ 0 ] + mw[ 1 ] * mw[ 1 ] + mw[ 2 ] * mw[ 2 ] ) : 1.0f;
				for( int k = 0; k < 3; ++k )
					x.pos[ k ] = w[ k ] - d[ k ] * behind * unit, x.dir[ k ] = d[ k ];
				x.front = behind * unit;
			}
			else
			{
				float local[ 3 ] = { 0, 0, 1 }; // Through Hole: straight out of the mask's front
				if( aim == 2 )
				{
					const float yaw = values[ P_LC_AIM_YAW ] * kDeg * ( side == 1 && mirror ? -1.0f : 1.0f ), pitch = values[ P_LC_AIM_PITCH ] * kDeg;
					local[ 0 ] = std::cos( pitch ) * std::sin( yaw ), local[ 1 ] = std::sin( pitch ), local[ 2 ] = std::cos( pitch ) * std::cos( yaw );
				}
				float lp[ 3 ];
				for( int k = 0; k < 3; ++k )
					lp[ k ] = open[ k ] - local[ k ] * behind;
				if( anchored )
				{
					anchor.Point( lp, x.pos );
					anchor.Dir( local, x.dir );
					float wo[ 3 ];
					anchor.Point( open, wo );
					float len = 0;
					for( int k = 0; k < 3; ++k )
						len += ( wo[ k ] - x.pos[ k ] ) * ( wo[ k ] - x.pos[ k ] );
					x.front = std::sqrt( len );
					Mat4::Norm( x.dir );
				}
				else
				{
					std::memcpy( x.pos, lp, sizeof( lp ) );
					std::memcpy( x.dir, local, sizeof( local ) );
					x.front = behind;
				}
			}
			if( !On( P_LC_FRONT_ONLY ) )
				x.front = -1.0f;
			const unsigned col = side == 1 && !On( P_LC_LINK ) ? P_LC_RCOL_H : P_LC_LCOL_H;
			float c[ 3 ];
			ColourDrift( col, float( clk.lightHue[ 2 ] ), c );
			for( int k = 0; k < 3; ++k )
				x.color[ k ] = std::max( 0.0f, c[ k ] ) * gain * Alpha( col );
			x.coneDeg  = values[ P_LC_CONE ];
			x.coneSoft = values[ P_LC_CONE_SOFT ];
		}
	}

	// ---- Stage 5 copies: clones around the mask (each clone = the mask's own transform, moved and turned
	// about the mask's centre), LOD by projected size; ghosts = past poses; echo trail settings.
	void BuildCopies( FrameState& fs, const Mat4& M, const float* pos, float scale, float dt )
	{
		fs.inst.clear();
		fs.ghosts.clear();
		const int layout = Opt( P_CLONE_LAYOUT );
		// turns (the param is deg/s); the tunnel reads it at x0.3 and x depth, so it wraps at 10 turns (keeping its sign)
		clk.cloneOrbit   = std::fmod( clk.cloneOrbit + dt * values[ P_CLONE_ORBIT ] / 360.0, 10.0 );
		const float cloneOrbit = float( clk.cloneOrbit );
		if( layout > 0 && LogVerbose() )
		{
			cloneLogDeg += dt * values[ P_CLONE_ORBIT ], cloneLogClock += dt;
			if( cloneLogClock >= 1.0f )
			{
				Log( "CLONES Orbit %.1f deg/s: the ring turned %.1f deg in %.2f s", values[ P_CLONE_ORBIT ], cloneLogDeg, cloneLogClock );
				cloneLogDeg = cloneLogClock = 0.0f;
			}
		}
		const float dist = PV( P_DISTANCE ), fovY = PV( P_FOV ) * kDeg;
		const float bias = values[ P_LOD_BIAS ];
		// projected radius (px) of a copy with scale s at camera distance d -> LOD 0..3
		auto lodFor = [ & ]( float s, float d ) {
			const float px = 1.2f * s / ( std::max( d, 0.05f ) * std::tan( fovY * 0.5f ) ) * float( fs.height ) * 0.5f * bias;
			return px > 350.0f ? 0 : px > 140.0f ? 1 : px > 50.0f ? 2 : 3;
		};
		auto camDist = [ & ]( const float* c ) {
			float d = 0;
			for( int k = 0; k < 3; ++k )
				d += ( c[ k ] - fs.camPos[ k ] ) * ( c[ k ] - fs.camPos[ k ] );
			return std::sqrt( d );
		};
		if( layout > 0 )
		{
			const int n       = std::max( 1, std::min( 64, (int)std::lround( values[ P_CLONE_COUNT ] ) ) );
			const float sp    = values[ P_CLONE_SPACING ], cs = values[ P_CLONE_SCALE ];
			const float step  = values[ P_CLONE_SPIN_STEP ] * kDeg, hueSpread = values[ P_CLONE_HUE ];
			const float orbit = cloneOrbit * 6.2831853f;
			const bool keep   = On( P_KEEP_ORIGINAL );
			const int cols = (int)std::ceil( std::sqrt( float( n ) ) ), rows = ( n + cols - 1 ) / cols;
			const int rings = ( n + 5 ) / 6;
			const float depth = std::max( 1, rings ) * sp * 1.5f;
			for( int i = 0; i < n; ++i )
			{
				float off[ 3 ] = { 0, 0, 0 }, yaw = step * float( i ), keep = 1.0f, fogKeep = 1.0f;
				switch( layout )
				{
				case 1: // ring facing the camera
				{
					const float a = 6.2831853f * float( i ) / float( n ) + orbit;
					off[ 0 ] = std::cos( a ) * sp, off[ 1 ] = std::sin( a ) * sp;
					break;
				}
				case 2: // grid
					off[ 0 ] = ( float( i % cols ) - 0.5f * float( cols - 1 ) ) * sp;
					off[ 1 ] = ( 0.5f * float( rows - 1 ) - float( i / cols ) ) * sp;
					if( keep && std::fabs( off[ 0 ] ) < 1e-4f && std::fabs( off[ 1 ] ) < 1e-4f )
						continue; // the original already sits in the middle cell
					break;
				case 3: // tunnel: rings of six flying toward the camera
				{
					const int r   = i / 6, k = i % 6;
					const float a = 6.2831853f * float( k ) / 6.0f + float( r ) * 0.5f + orbit * 0.3f;
					off[ 0 ] = std::cos( a ) * sp, off[ 1 ] = std::sin( a ) * sp;
					off[ 2 ] = -depth + std::fmod( float( r ) * sp * 1.5f + std::fabs( cloneOrbit ) * depth, depth );
					// W15: a ring fades in at the back and out at the front, so its recycle (front -> back) is never seen.
					// Tunnel Fade = the share of the tunnel the two fade zones take together (0 = the old pop). With Depth
					// Fog on, the back end arrives out of the fog instead of dissolving in.
					const float u = ( off[ 2 ] + depth ) / depth, zone = 0.5f * values[ P_TUNNEL_FADE ];
					if( zone > 1e-3f )
					{
						const float back = SmoothStep( 0.0f, zone, u ), front = 1.0f - SmoothStep( 1.0f - zone, 1.0f, u );
						keep    = values[ P_DEPTH_FOG ] > 0.0f ? front : front * back;
						fogKeep = values[ P_DEPTH_FOG ] > 0.0f ? back : 1.0f;
					}
					break;
				}
				default: // carousel: a horizontal wheel, each copy facing out
				{
					const float a = 6.2831853f * float( i ) / float( n ) + orbit;
					off[ 0 ] = std::sin( a ) * sp, off[ 2 ] = std::cos( a ) * sp;
					yaw += a;
					break;
				}
				}
				// T(pos + off) * RotY(yaw) * Scale(cs) * T(-pos) * M
				const Mat4 C = Mat4::Translate( pos[ 0 ] + off[ 0 ], pos[ 1 ] + off[ 1 ], pos[ 2 ] + off[ 2 ] ) * Mat4::RotY( yaw ) * Mat4::Scale( cs )
				             * Mat4::Translate( -pos[ 0 ], -pos[ 1 ], -pos[ 2 ] ) * M;
				Instance in;
				std::memcpy( in.m, C.m, sizeof( in.m ) );
				in.hue = hueSpread * float( i ) / float( n );
				in.keep = keep, in.fogKeep = fogKeep;
				float c[ 3 ] = { pos[ 0 ] + off[ 0 ], pos[ 1 ] + off[ 1 ], pos[ 2 ] + off[ 2 ] };
				in.lod = lodFor( scale * cs, camDist( c ) );
				fs.inst.push_back( in );
			}
			if( keep )
			{
				Instance in;
				std::memcpy( in.m, M.m, sizeof( in.m ) );
				in.lod = 0;
				fs.inst.insert( fs.inst.begin(), in );
			}
			if( fs.inst.empty() ) // nothing to draw must not fall back to the plain single draw
			{
				Instance in;
				std::memcpy( in.m, M.m, sizeof( in.m ) );
				in.alpha = 0;
				fs.inst.push_back( in );
			}
		}
		// ghosts: the pose history holds one model matrix per frame
		poseHistory[ poseHead ] = M;
		poseHead                = ( poseHead + 1 ) % kPoseHistory;
		poseCount               = std::min( poseCount + 1, kPoseHistory );
		const int ghosts  = std::min( 8, (int)std::lround( values[ P_GHOSTS ] ) );
		const int gap     = std::max( 1, (int)std::lround( values[ P_GHOST_SPACING ] ) );
		const int curve   = Opt( P_TRAIL_CURVE );
		const float smear = values[ P_TRAIL_SMEAR ];
		const float life  = float( ( ghosts + 1 ) * gap ) * ( curve == 2 ? 1.5f : 1.0f ); // frames
		for( int g = 1; g <= ghosts; ++g ) // newest first (see Renderer: ghosts write depth)
		{
			const int back = g * gap;
			if( back >= poseCount )
				continue;
			const Mat4& P = poseHistory[ ( poseHead - 1 - back + 2 * kPoseHistory ) % kPoseHistory ];
			Instance in;
			std::memcpy( in.m, P.m, sizeof( in.m ) );
			// Smear: the ghost stretches toward the next newer pose (the live mask for the first), see kMainVS
			std::memcpy( in.next, poseHistory[ ( poseHead - 1 - ( g - 1 ) * gap + 2 * kPoseHistory ) % kPoseHistory ].m, sizeof( in.next ) );
			in.age   = std::min( 1.0f, float( back ) / life );
			in.alpha = 0.7f * TrailFade( values[ P_GHOST_FADE ], float( back ) / float( gap ), in.age, curve );
			in.lod   = std::max( 1, lodFor( scale, dist ) );
			if( in.alpha > 0.002f )
				fs.ghosts.push_back( in );
		}
		TrailState& t = fs.trail;
		t.echoes      = std::min( 7, (int)std::lround( values[ P_TRAIL_ECHOES ] ) );
		t.delay       = std::max( 1, (int)std::lround( values[ P_TRAIL_DELAY ] ) );
		t.decay       = values[ P_TRAIL_DECAY ];
		t.offset[ 0 ] = values[ P_TRAIL_OFFSET_X ];
		t.offset[ 1 ] = values[ P_TRAIL_OFFSET_Y ];
		t.smear       = smear;
		t.ghostSmear  = smear;
		t.blur        = values[ P_TRAIL_BLUR ];
		t.dust        = values[ P_TRAIL_DUST ];
		t.curve       = curve;
		t.glow        = values[ P_TRAIL_GLOW ] * Alpha( P_TRAIL_COL_H );
		t.colorMix    = values[ P_TRAIL_COL_MIX ] * Alpha( P_TRAIL_COL_H );
		Colour( P_TRAIL_COL_H, t.color );
		// dust grain + drift: pixels scale with the output height (the dials are set at 1080p)
		const float hpx   = float( std::max( 1, fs.height ) );
		t.grainPx         = values[ P_TRAIL_GRAIN ] * hpx / 1080.0f;
		const float ddir  = values[ P_TRAIL_DRIFT_DIR ] * kDeg, dspeed = values[ P_TRAIL_DRIFT ]; // screen heights / s
		const float aspect = float( std::max( 1, fs.width ) ) / hpx;
		t.drift[ 0 ]      = std::cos( ddir ) * dspeed * dt / aspect, t.drift[ 1 ] = std::sin( ddir ) * dspeed * dt;
		for( int k = 0; k < 2; ++k ) // the grain pattern moves with the dust; the grain hash repeats every 256 cells
		{
			const double cells = ( k == 0 ? std::cos( ddir ) : std::sin( ddir ) ) * dspeed * dt * hpx / std::max( 0.5f, t.grainPx );
			clk.trailGrain[ k ] = Wrap( clk.trailGrain[ k ] - cells, 256.0 );
			t.grainPhase[ k ]   = float( clk.trailGrain[ k ] );
		}
		// ghosts: grain in fitted object units (one grain ~ Trail Grain pixels on the mask), drift in world units
		const float worldPerPx = 2.0f * dist * std::tan( fovY * 0.5f ) / hpx;
		t.ghostGrain           = t.grainPx * worldPerPx / std::max( 1e-4f, scale );
		float right[ 3 ] = { fs.view[ 0 ], fs.view[ 4 ], fs.view[ 8 ] }, up[ 3 ] = { fs.view[ 1 ], fs.view[ 5 ], fs.view[ 9 ] };
		const float reach = dspeed * 2.0f * dist * std::tan( fovY * 0.5f ) * life / 60.0f; // how far the drift carries a ghost at the trail's end
		for( int k = 0; k < 3; ++k )
			t.ghostDrift[ k ] = ( right[ k ] * std::cos( ddir ) + up[ k ] * std::sin( ddir ) ) * reach;
	}

	// Trail fade (W15). decay = the master (Trail Decay / Ghost Fade) per copy; k = age in copies; a = age along the
	// trail (0..1). Linear fades to nothing at the end, Soft holds and then eases out, Long Tail halves the decay
	// and leaves a faint tail (the trail is also 1.5x longer).
	static float TrailFade( float decay, float k, float a, int curve )
	{
		const float d = std::max( 0.0f, decay );
		if( curve == 1 )
			return std::pow( d, k ) * ( 1.0f - SmoothStep( 0.3f, 1.0f, a ) );
		if( curve == 2 )
			return std::pow( d, 0.5f * k ) * ( 1.0f - a ) * ( 1.0f - a );
		return std::pow( d, k ) * ( 1.0f - a );
	}

	// ---- Stage 4 shape. Shatter: flight time rises while broken and runs back to 0 over Reform Time
	// (the shards retrace their path). Dust: progress rises on Blow To Dust and falls on Dust Reform.
	void BuildShape( FrameState& fs, float pulse, float dt )
	{
		ShapeState& s   = fs.shape;
		clk.slice       = Wrap( clk.slice + dt * 2.0, kTwoPi ); // Slice Wave: radians
		clk.dustCurl    = Wrap( clk.dustCurl + dt * 0.25, 64.0 ); // the dust wind's noise tiles every 64
		s.slicePhase    = float( clk.slice );
		s.curlY         = float( clk.dustCurl );
		s.dt            = dt;
		s.reformTime    = values[ P_REFORM_TIME ];
		s.shardCount    = std::max( 8, std::min( 512, (int)std::lround( values[ P_SHARD_COUNT ] ) ) );
		s.shatterMode   = std::clamp( Opt( P_SHATTER_MODE ), 0, 2 );
		const int every = Opt( P_SHATTER_BEAT ) == 0 ? 0 : 1 << ( Opt( P_SHATTER_BEAT ) - 1 );
		if( every > 0 && beatCount != lastShatterBeat && beatCount % every == 0 )
		{
			lastShatterBeat = beatCount;
			shatterRequest  = 1;
			autoReformAt    = 0.5f * every * std::max( 0.1f, beatPeriod ); // fly for half the cycle, then come back
		}
		if( shatterRequest == 1 )
			shatterState = 1, shatterT = 0.0f, shatterClock = 0.0f, ++shatterGen; // W26: a new flight for Bounce
		else if( shatterRequest == 2 && shatterState == 1 )
			shatterState = 2, reformFrom = shatterT, reformClock = 0.0f;
		if( shatterRequest == 1 && every == 0 )
			autoReformAt = -1.0f;
		shatterRequest = 0;
		// W28 Physics Speed (Wow bundles; the classic ones keep the W26 pace): flight, bounce and reform run on physics time.
		// The beat cycle's timing (fly for half the cycle, reform within 0.45 of it) stays in show time.
		const float ps = LOADEHR_WOW ? values[ P_PHYSICS_SPEED ] : 1.0f;
		if( shatterState == 1 )
		{
			shatterT = std::min( shatterT + dt * ps, 4.0f );
			shatterClock += dt;
			if( autoReformAt > 0.0f && shatterClock >= autoReformAt )
				shatterState = 2, reformFrom = shatterT, reformClock = 0.0f;
		}
		else if( shatterState == 2 )
		{
			const float slow = s.reformTime / std::max( ps, 0.05f );
			const float dur  = every > 0 ? std::min( slow, 0.45f * every * std::max( 0.1f, beatPeriod ) ) : slow;
			reformClock += dt;
			float u  = std::min( 1.0f, reformClock / std::max( 0.02f, dur ) );
			u        = u * u * ( 3.0f - 2.0f * u );
			shatterT = reformFrom * ( 1.0f - u );
			if( u >= 1.0f )
				shatterState = 0, shatterT = 0.0f;
		}
		s.shatterT       = shatterT;
		s.shatterForce   = values[ P_SHATTER_FORCE ];
		s.shatterGravity = values[ P_SHATTER_GRAVITY ];
		s.shardSpin      = values[ P_SHARD_SPIN ];
		s.bounce         = Opt( P_SHATTER_PHYSICS ) == 1;
		s.bounciness     = values[ P_BOUNCINESS ];
		s.friction       = values[ P_FLOOR_FRICTION ];
		s.shatterGen     = shatterGen;
		s.sliceCount = std::max( 2, (int)std::lround( values[ P_SLICE_COUNT ] ) ), s.sliceGap = values[ P_SLICE_GAP ];
		s.sliceAxis = Opt( P_SLICE_AXIS ), s.sliceWave = values[ P_SLICE_WAVE ];
		s.voxel = values[ P_VOXEL ], s.voxelSize = values[ P_VOXEL_SIZE ], s.lowPoly = values[ P_LOW_POLY ], s.lowPolySize = values[ P_LOW_POLY_SIZE ];
		BuildDeform( s, pulse, dt );
		BuildDust( s, dt );
		BuildLowPoly( s, dt );
		static const int kDust[ 5 ] = { 25000, 50000, 100000, 200000, 400000 };
		s.dustCount = kDust[ std::min( 4, std::max( 0, Opt( P_DUST_COUNT ) ) ) ];
		s.dustWind  = values[ P_DUST_WIND ];
		s.dustSize  = values[ P_DUST_SIZE ];
		s.contour   = values[ P_CONTOURS ];
		s.contourMode    = Opt( P_CONTOUR_MODE );
		s.contourSpacing = values[ P_CONTOUR_SPACING ];
		s.contourWidth   = values[ P_CONTOUR_WIDTH ];
		clk.contour      = Wrap( clk.contour + dt * values[ P_CONTOUR_SPEED ], 1.0 ); // in spacings
		s.contourScroll  = float( clk.contour );
		s.contour *= Alpha( P_CONTOUR_COL_H );
		Colour( P_CONTOUR_COL_H, s.contourColor );
		s.on = s.shatterT > 0.0f || s.melt > 0.0f || s.sliceGap > 0.0f || s.sliceWave > 0.0f || s.voxel > 0.0f || s.lowPoly > 0.0f || s.twist != 0.0f
		    || s.bend != 0.0f || s.ripple > 0.0f || s.deform;
	}

	// ---- W2 Deform group: every deform x Deform Amount (+ Audio > Deform), in the DEFORM program twins. deform = any on (else the
	// plain programs draw, byte for byte as before). Clocks (W14): Ripple wraps at 2 pi, Noise Warp's drift at 256 lattice cells
	// (the x / y / z drift 1 : 0.5 : 0.25 = whole 64-periods), the Glitch step at 1000 steps; Jelly is a spring, not a phase.
	void BuildDeform( ShapeState& s, float pulse, float dt )
	{
		const float k = std::max( 0.0f, values[ P_DEFORM_AMOUNT ] + values[ P_AUDIO_DEFORM ] * 0.5f * audio[ 0 ] );
		s.deformAxis  = Opt( P_DEFORM_AXIS );
		s.twist = values[ P_TWIST ] * k, s.bend = values[ P_BEND ] * k;
		s.melt        = values[ P_MELT ] * k, s.drip = values[ P_DRIP ];
		s.ripple      = values[ P_RIPPLE ] * k * ( 1.0f + values[ P_RIPPLE_BEAT ] * pulse * 2.0f );
		s.rippleFreq  = values[ P_RIPPLE_FREQ ];
		clk.ripple    = Wrap( clk.ripple + dt * values[ P_RIPPLE_SPEED ], kTwoPi );
		s.ripplePhase = float( clk.ripple );
		s.taper       = values[ P_TAPER ] * k;
		s.squash      = ( values[ P_SQUASH ] + values[ P_SQUASH_BEAT ] * 0.5f * pulse ) * k;
		s.shear       = values[ P_SHEAR ] * k;
		s.bulge       = values[ P_BULGE ] * k;
		s.spherize    = std::min( 1.0f, values[ P_SPHERIZE ] * k );
		s.warp        = values[ P_NOISE_WARP ] * k;
		s.warpScale   = values[ P_WARP_SCALE ];
		clk.warp      = Wrap( clk.warp + dt * values[ P_WARP_SPEED ], 256.0 );
		s.warpPhase   = float( clk.warp );
		s.glitchShift = values[ P_GLITCH_SHIFT ] * k;
		s.glitchBands = std::max( 2.0f, std::floor( values[ P_GLITCH_BANDS ] + 0.5f ) );
		clk.glitch    = Wrap( clk.glitch + dt * values[ P_GLITCH_RATE ], 1000.0 );
		s.glitchStep  = std::floor( float( clk.glitch ) );
		const int fold = std::clamp( Opt( P_MIRROR_FOLD ), 0, 3 );
		for( int a = 0; a < 3; ++a )
			s.foldAxes[ a ] = a < fold ? 1.0f : 0.0f;
		s.foldK = fold > 0 ? std::min( 1.0f, k ) : 0.0f;
		s.foldN = fold > 0 && s.foldK > 0.0f ? 1 << fold : 1;
		const float j = values[ P_JELLY ] * k;
		for( int a = 0; a < 3; ++a )
			s.jellyLin[ a ] = jelly.lin[ a ] * j, s.jellyAng[ a ] = jelly.ang[ a ] * j;
		s.jellyScale = jelly.scale * j;
		s.deform = s.twist != 0.0f || s.bend != 0.0f || s.melt > 0.0f || s.ripple > 0.0f || s.taper != 0.0f || s.squash != 0.0f || s.shear != 0.0f ||
		           s.bulge != 0.0f || s.spherize > 0.0f || s.warp > 0.0f || s.glitchShift > 0.0f || s.foldK > 0.0f || j > 0.0f;
		(void)dt;
	}

	// the Jelly spring: the mask's pose this frame (anchor = mask space) -> its acceleration in mask space drives a damped spring
	// (1.6 Hz, damping 0.22: it wobbles a few times on a stop), integrated at 240 Hz. Always integrated (the dial only scales the
	// result), so turning Jelly up mid-move never jumps.
	void StepJelly( const Mat4& anchor, float dt )
	{
		Jelly& J = jelly;
		float pos[ 3 ], R[ 9 ], sc = 0;
		const float o[ 3 ] = { 0, 0, 0 };
		anchor.Point( o, pos );
		for( int c = 0; c < 3; ++c )
		{
			float l = 0;
			for( int r = 0; r < 3; ++r )
				l += anchor.m[ c * 4 + r ] * anchor.m[ c * 4 + r ];
			l = std::sqrt( l );
			sc += l / 3.0f;
			for( int r = 0; r < 3; ++r )
				R[ c * 3 + r ] = anchor.m[ c * 4 + r ] / std::max( l, 1e-9f );
		}
		if( dt <= 1e-5f )
			return;
		float vel[ 3 ], acc[ 3 ], w[ 3 ] = {}, alpha[ 3 ];
		for( int k = 0; k < 3; ++k )
			vel[ k ] = ( pos[ k ] - J.prevPos[ k ] ) / dt, acc[ k ] = ( vel[ k ] - J.prevVel[ k ] ) / dt;
		// angular velocity: the rotation from last frame to this one, R * prevR^T, as axis x angle / dt (world)
		float D[ 9 ];
		for( int c = 0; c < 3; ++c )
			for( int r = 0; r < 3; ++r )
			{
				float v = 0;
				for( int k = 0; k < 3; ++k )
					v += R[ k * 3 + r ] * J.prevR[ k * 3 + c ];
				D[ c * 3 + r ] = v;
			}
		const float tr = std::clamp( ( D[ 0 ] + D[ 4 ] + D[ 8 ] - 1.0f ) * 0.5f, -1.0f, 1.0f ), ang = std::acos( tr );
		if( ang > 1e-6f )
		{
			const float s2 = 2.0f * std::sin( ang );
			w[ 0 ] = ( D[ 5 ] - D[ 7 ] ) / s2 * ang / dt, w[ 1 ] = ( D[ 6 ] - D[ 2 ] ) / s2 * ang / dt, w[ 2 ] = ( D[ 1 ] - D[ 3 ] ) / s2 * ang / dt;
		}
		for( int k = 0; k < 3; ++k )
			alpha[ k ] = ( w[ k ] - J.prevW[ k ] ) / dt;
		const float sv = ( sc - J.prevScale ) / dt, sa = ( sv - J.prevScaleV ) / dt;
		const bool drive = J.seen >= 2;
		J.seen = std::min( J.seen + 1, 2 );
		std::memcpy( J.prevPos, pos, sizeof( pos ) ), std::memcpy( J.prevVel, vel, sizeof( vel ) ), std::memcpy( J.prevW, w, sizeof( w ) ), std::memcpy( J.prevR, R, sizeof( R ) );
		J.prevScale = sc, J.prevScaleV = sv;
		// into mask space (R^T, per mask unit); clamped: a teleport (a pose jump) is not a shake
		float aL[ 3 ] = {}, alL[ 3 ] = {};
		for( int k = 0; k < 3; ++k )
			for( int r = 0; r < 3; ++r )
				aL[ k ] += R[ k * 3 + r ] * acc[ r ] / std::max( sc, 1e-6f ), alL[ k ] += R[ k * 3 + r ] * alpha[ r ];
		const float om = 2.0f * 3.14159265f * 1.6f, ze = 0.22f;
		const int n    = std::max( 1, (int)std::ceil( dt * 240.0f ) );
		const float h  = dt / n;
		for( int i = 0; i < n; ++i )
			for( int k = 0; k < 3; ++k )
			{
				const float fl = drive ? std::clamp( -aL[ k ] * 0.04f, -60.0f, 60.0f ) : 0.0f, fa = drive ? std::clamp( -alL[ k ] * 0.04f, -60.0f, 60.0f ) : 0.0f;
				J.linV[ k ] += ( fl - om * om * J.lin[ k ] - 2.0f * ze * om * J.linV[ k ] ) * h, J.lin[ k ] += J.linV[ k ] * h;
				J.angV[ k ] += ( fa - om * om * J.ang[ k ] - 2.0f * ze * om * J.angV[ k ] ) * h, J.ang[ k ] += J.angV[ k ] * h;
				J.lin[ k ] = std::clamp( J.lin[ k ], -0.6f, 0.6f ), J.ang[ k ] = std::clamp( J.ang[ k ], -1.0f, 1.0f );
			}
		const float fs = drive ? std::clamp( -sa / std::max( sc, 1e-6f ) * 0.04f, -60.0f, 60.0f ) : 0.0f;
		for( int i = 0; i < n; ++i )
			J.scaleV += ( fs - om * om * J.scale - 2.0f * ze * om * J.scaleV ) * h, J.scale += J.scaleV * h;
		J.scale = std::clamp( J.scale, -0.5f, 0.5f );
	}

	// ---- W21 letters: every glyph / logo shape moves as one piece (Type On, Wave, Spin, Hop). Pieces in reading order.
	void BuildLetters( FrameState& fs, float dt )
	{
		fs.letterXf.clear();
		const std::shared_ptr< GpuModel >& cur = slots[ 0 ].current;
		const CpuGeometry* g = cur && cur->geometry && IsGenSpec( cur->path ) ? cur->geometry.get() : nullptr;
		const int N          = g ? g->groupCount : 0;
		const float speed = values[ P_TYPE_SPEED ], wave = values[ P_LETTER_WAVE ], spin = values[ P_LETTER_SPIN ];
		const float S     = std::min( values[ P_LETTER_STAGGER ], 0.98f );
		// clocks (W14): the type cycle wraps at 1 (the restart), the wave at 1 cycle, the spin at 360 (whole-number factors)
		const double cycle = double( std::max( N, 1 ) ) + 2.0 * speed; // letters: type them all, hold 2 s
		clk.typeOn     = speed > 0.0f ? Wrap( clk.typeOn + dt * speed / cycle, 1.0 ) : 0.0;
		clk.letterWave = Wrap( clk.letterWave + dt * values[ P_WAVE_SPEED ], 1.0 );
		clk.letterSpin = Wrap( clk.letterSpin + dt * spin, 360.0 );
		const int every = Opt( P_LETTER_HOP ) == 0 ? 0 : 1 << ( Opt( P_LETTER_HOP ) - 1 );
		if( every > 0 && beatCount != lastHopBeat && beatCount % every == 0 )
			lastHopBeat = beatCount, hopT = 0.0f;
		hopT = std::min( hopT + dt, 1000.0f );
		const float hopDur = std::min( 0.4f, 0.9f * every * std::max( 0.1f, beatPeriod ) ), hopSpread = 0.6f * S;
		const float typeT  = speed > 0.0f ? float( std::min( 1.0, clk.typeOn * cycle / std::max( N, 1 ) ) ) : values[ P_TYPE_ON ];
		const bool active  = N > 0 && ( typeT < 0.999f || wave > 0.0f || clk.letterSpin != 0.0 || ( every > 0 && hopT < hopDur + hopSpread ) );
		if( !active )
			return;
		float hAvg = 0;
		for( int i = 0; i < N; ++i )
			hAvg += g->groupBox[ i * 6 + 4 ] - g->groupBox[ i * 6 + 1 ];
		hAvg /= float( N );
		fs.letterXf.resize( size_t( N ) * 12 );
		for( int i = 0; i < N; ++i )
		{
			const float* b = &g->groupBox[ i * 6 ];
			float c[ 3 ];
			for( int k = 0; k < 3; ++k )
				c[ k ] = 0.5f * ( b[ k ] + b[ 3 + k ] );
			const float key = N > 1 ? float( i ) / float( N - 1 ) : 0.0f;
			float u         = std::clamp( ( typeT - S * key ) / ( 1.0f - S ), 0.0f, 1.0f );
			u               = u * u * ( 3.0f - 2.0f * u );
			float y         = -( 1.0f - u ) * 0.5f * hAvg; // letters rise into place as they type on
			y += wave * 0.5f * hAvg * std::sin( 6.2831853f * float( clk.letterWave - c[ 0 ] * 0.35f ) );
			if( every > 0 )
				y += values[ P_HOP_HEIGHT ] * hAvg * std::sin( 3.1415927f * std::clamp( ( hopT - key * hopSpread ) / hopDur, 0.0f, 1.0f ) );
			// spin about the letter's vertical axis; Separate: whole-number speed factor -2, -1, 1, 2 per letter
			float f = 1.0f;
			if( Opt( P_SPIN_MODE ) == 1 )
			{
				const uint32_t h = ( uint32_t( i ) * 2654435761u ) >> 28;
				f                = float( int( h & 3u ) - 2 + ( ( h & 3u ) >= 2u ? 1 : 0 ) );
			}
			const float a = float( clk.letterSpin ) * f * kDeg * 0.5f;
			float* x      = &fs.letterXf[ size_t( i ) * 12 ];
			x[ 0 ] = c[ 0 ], x[ 1 ] = c[ 1 ], x[ 2 ] = c[ 2 ], x[ 3 ] = u;
			x[ 4 ] = 0.0f, x[ 5 ] = std::sin( a ), x[ 6 ] = 0.0f, x[ 7 ] = std::cos( a );
			x[ 8 ] = 0.0f, x[ 9 ] = y, x[ 10 ] = 0.0f, x[ 11 ] = 0.0f;
		}
		fs.shape.on = true; // the letters draw through the soup stage
	}

	// ---- Blow To Dust: EVERY press blows. If dust is already out, the points snap home and burst again. Dust
	// Reform brings it back at any time. Dust On Beat blows for half the cycle and reforms for the other half;
	// Dust Hold reforms by itself that many seconds after the blow has finished. The Dust slider scrubs by hand
	// until the next press (a press takes over from wherever the slider left the mask).
	void BuildDust( ShapeState& s, float dt )
	{
		const int every = Opt( P_DUST_BEAT ) == 0 ? 0 : 1 << ( Opt( P_DUST_BEAT ) - 1 );
		const float cycle = 0.5f * every * std::max( 0.1f, beatPeriod );
		if( every > 0 && beatCount != lastDustBeat && beatCount % every == 0 )
			lastDustBeat = beatCount, dustPress = 1;
		s.dustRestart = false;
		if( dustPress != 0 && LogVerbose() )
			Log( "DUST %s%s: cut %.2f (%s), target %.0f", dustPress == 1 ? "Blow To Dust" : "Dust Reform", every > 0 && dustPress == 1 ? " (on beat)" : "",
			     dustManual ? values[ P_DUST ] : dustProgress, dustManual ? "slider" : "event", dustTarget );
		if( dustPress == 1 )
		{
			s.dustRestart = ( dustManual ? values[ P_DUST ] : dustProgress ) > 0.0f || dustReturnTimer > 0.0f;
			dustProgress  = 0.0f;
			dustTarget    = 1.0f;
			dustClock     = 0.0f;
			dustAuto      = every > 0;
			dustManual    = false;
		}
		else if( dustPress == 2 )
		{
			if( dustManual )
				dustProgress = values[ P_DUST ];
			dustTarget = 0.0f;
			dustManual = false;
		}
		dustPress = 0;
		const float riseDur = dustAuto ? std::min( 2.5f, 0.9f * cycle ) : 2.5f;
		const float fallDur = dustAuto ? std::min( s.reformTime, 0.9f * cycle ) : s.reformTime;
		if( dustTarget > 0.0f )
		{
			dustClock += dt;
			const float hold = values[ P_DUST_HOLD ];
			if( dustAuto ? dustClock >= cycle : ( hold > 0.0f && dustClock >= riseDur + hold ) )
				dustTarget = 0.0f;
		}
		dustProgress = dustTarget > dustProgress ? std::min( dustTarget, dustProgress + dt / riseDur )
		                                         : std::max( dustTarget, dustProgress - dt / std::max( 0.05f, fallDur ) );
		s.dustCut       = dustManual ? values[ P_DUST ] : dustProgress;
		dustReturnTimer = s.dustCut > 0.0f ? s.reformTime * 1.5f + 0.5f : std::max( 0.0f, dustReturnTimer - dt );
		s.dustReturning = dustReturnTimer > 0.0f;
	}

	// ---- Low poly: the grid turns (static angle + integrated spin); facets tumble about their own centres
	void BuildLowPoly( ShapeState& s, float dt )
	{
		for( int k = 0; k < 3; ++k )
			clk.lowPolySpin[ k ] = std::fmod( clk.lowPolySpin[ k ] + values[ P_LP_SPIN_X + k ] * dt, 360.0 );
		// grid = Rg0 * G^T, G = the grid's rotation (the fixed tilt Rg0 keeps the facets irregular)
		const Mat4 G = Mat4::RotZ( ( values[ P_LP_ROT_Z ] + float( clk.lowPolySpin[ 2 ] ) ) * kDeg ) * Mat4::RotY( ( values[ P_LP_ROT_Y ] + float( clk.lowPolySpin[ 1 ] ) ) * kDeg )
		             * Mat4::RotX( ( values[ P_LP_ROT_X ] + float( clk.lowPolySpin[ 0 ] ) ) * kDeg );
		static const float Rg0[ 3 ][ 3 ] = { { 0.36f, 0.48f, -0.8f }, { -0.8f, 0.6f, 0.0f }, { 0.48f, 0.64f, 0.6f } }; // columns
		for( int c = 0; c < 3; ++c )
			for( int r = 0; r < 3; ++r )
			{
				float v = 0; // (Rg0 * G^T)(r, c) = sum_k Rg0(r, k) * G(c, k)
				for( int k = 0; k < 3; ++k )
					v += Rg0[ k ][ r ] * G.m[ k * 4 + c ];
				s.lowPolyGrid[ c * 3 + r ] = v;
			}
		s.tumble       = Opt( P_TUMBLE );
		s.tumbleRandom = values[ P_TUMBLE_RANDOM ];
		s.tumbleSway   = values[ P_TUMBLE_SWAY ];
		s.facetGap     = values[ P_FACET_GAP ];
		// angle = the steady turn (speed x (1 - Sway)) + Sway x 35 deg x sin(rocking phase). Both are integrated and wrap
		// at 360: facet speeds are whole numbers (-2..2), so every facet's angle stays continuous across the wrap,
		// and dragging Sway never jumps the facets.
		if( s.tumble > 0 )
			for( int k = 0; k < 3; ++k )
			{
				clk.tumble[ k ]    = std::fmod( clk.tumble[ k ] + values[ P_TUMBLE_X + k ] * ( 1.0 - s.tumbleSway ) * dt, 360.0 );
				clk.tumbleSway[ k ] = std::fmod( clk.tumbleSway[ k ] + values[ P_TUMBLE_X + k ] * dt, 360.0 );
			}
		// beat flips: base = the angle of the flips already done, then one eased kick
		const int every = Opt( P_TUMBLE_BEAT ) == 0 ? 0 : 1 << ( Opt( P_TUMBLE_BEAT ) - 1 );
		if( every > 0 && beatCount != lastFlipBeat && beatCount % every == 0 )
		{
			lastFlipBeat = beatCount;
			flipBase     = std::fmod( flipBase + flipDeg, 360.0f );
			flipDeg      = Opt( P_TUMBLE_FLIP ) == 1 ? 180.0f : 90.0f;
			flipT        = 0.0f;
			flipDur      = std::min( 0.35f, 0.45f * every * std::max( 0.1f, beatPeriod ) );
		}
		flipT = std::min( flipT + dt, 1000.0f );
		// Tumble Reset: everything glides back to rest over a second
		float w = 1.0f;
		if( tumbleResetT >= 0.0f )
		{
			tumbleResetT += dt;
			const float u = std::min( 1.0f, tumbleResetT / 1.0f );
			w             = 1.0f - u * u * ( 3.0f - 2.0f * u );
			if( u >= 1.0f )
			{
				for( int k = 0; k < 3; ++k )
					clk.tumble[ k ] = clk.tumbleSway[ k ] = 0.0;
				flipBase     = -flipDeg;
				tumbleResetT = -1.0f;
				w            = 1.0f;
			}
		}
		for( int k = 0; k < 3; ++k )
			s.tumbleAng[ k ] = float( clk.tumble[ k ] ), s.tumblePhase[ k ] = float( clk.tumbleSway[ k ] );
		s.tumbleW  = w;
		s.flipOn   = every > 0 || std::fabs( std::fmod( flipBase + flipDeg + 720.0f, 360.0f ) ) > 1e-3f;
		s.flipBase = flipBase, s.flipT = flipT, s.flipDur = flipDur, s.flipDeg = flipDeg;
	}

	// ---- Stage 3 surface effects
	void BuildSurface( FrameState& fs, float pulse, float dt )
	{
		SurfaceState& u = fs.surf;
		// seconds, only for the random-step holo flicker / glitch (a wrap is one more random step)
		clk.surf        = Wrap( clk.surf + dt, 3600.0 );
		u.time          = float( clk.surf );
		clk.holo        = Wrap( clk.holo + dt * 4.0, kTwoPi ); // scanline crawl, radians
		u.holoPhase     = float( clk.holo );
		// twinkles; each flake's rate is a whole number of twinkles per 1000, so the wrap at 1000 is seamless
		clk.glitter     = Wrap( clk.glitter + dt * values[ P_GLITTER_TWINKLE ], 1000.0 );
		u.glitterPhase  = float( clk.glitter );
		// chrome ripple: the noise field tiles every 64 on each axis
		clk.chrome[ 0 ] = Wrap( clk.chrome[ 0 ] + dt * 0.6, 64.0 ), clk.chrome[ 1 ] = Wrap( clk.chrome[ 1 ] + dt * 0.3, 64.0 );
		u.chromeOff[ 0 ] = float( clk.chrome[ 0 ] ), u.chromeOff[ 1 ] = float( clk.chrome[ 1 ] );
		// film swirl and the pattern crawl are not periodic: over the last stretch before the wrap they cross-fade
		// into the field at (phase - period), which is exactly where they continue after it
		clk.film = Wrap( clk.film + dt * 0.07, kFilmLoop );
		clk.pattern = Wrap( clk.pattern + dt * values[ P_PAT_SPEED ], kPatLoop );
		u.filmT = float( clk.film ), u.filmT2 = float( clk.film - kFilmLoop ), u.filmX = LoopFade( clk.film, kFilmLoop );
		u.patT = float( clk.pattern ), u.patT2 = float( clk.pattern - kPatLoop ), u.patX = LoopFade( clk.pattern, kPatLoop );
		u.beat          = pulse;
		u.audioMid      = audio[ 1 ];
		u.film = values[ P_FILM ], u.filmThick = values[ P_FILM_THICK ], u.filmShift = values[ P_FILM_SHIFT ], u.filmSwirl = values[ P_FILM_SWIRL ];
		u.glitter = values[ P_GLITTER ], u.glitterDensity = values[ P_GLITTER_DENSITY ], u.glitterSize = values[ P_GLITTER_SIZE ];
		u.glitterMetal = values[ P_GLITTER_METAL ];
		u.chrome = values[ P_CHROME ], u.chromeRipple = values[ P_CHROME_RIPPLE ];
		u.glass = values[ P_GLASS ], u.glassIOR = values[ P_GLASS_IOR ], u.glassDisp = values[ P_GLASS_DISP ];
		Colour( P_GLASS_TINT_H, u.glassTint );
		for( float& c : u.glassTint ) // alpha = how strongly the glass is tinted
			c = 1.0f + ( c - 1.0f ) * Alpha( P_GLASS_TINT_H );
		Colour( P_PAT_COL_H, u.patColor );
		u.holo = values[ P_HOLO ], u.holoScan = values[ P_HOLO_SCAN ], u.holoFlicker = values[ P_HOLO_FLICKER ], u.holoSplit = values[ P_HOLO_SPLIT ];
		u.glitch = u.holo * values[ P_HOLO_GLITCH ] * pulse;
		Colour( P_HOLO_COL_H, u.holoColor );
		for( float& c : u.holoColor )
			c *= Alpha( P_HOLO_COL_H );
		u.heat = values[ P_HEAT ], u.heatTemp = values[ P_HEAT_TEMP ], u.heatSpread = values[ P_HEAT_SPREAD ];
		u.pattern = Opt( P_PATTERN ), u.patAmt = values[ P_PAT_AMOUNT ] * Alpha( P_PAT_COL_H ), u.patScale = values[ P_PAT_SCALE ];
		u.patBeat = values[ P_PAT_BEAT ], u.patAudio = values[ P_PAT_AUDIO ];
		u.on = u.film > 0 || u.glitter > 0 || u.chrome > 0 || u.glass > 0 || u.holo > 0 || u.heat > 0 || ( u.pattern > 0 && u.patAmt > 0 );
	}

	// ---- floor / wall, silhouette, bloom + flares
	void BuildPost( FrameState& fs, const Mat4& anchor, float scale )
	{
		PostState& p            = fs.post;
		const float origin[ 3 ] = { 0, 0, 0 };
		anchor.Point( origin, p.maskCenter );
		p.maskRadius = 1.2f * scale;
		p.shadowAmt  = values[ P_SHADOW_CATCHER ];
		p.shadowSoft = values[ P_SHADOW_SOFT ];
		p.floorLight = values[ P_FLOOR_LIGHT ];
		Colour( P_KEY_COL_H, p.floorLightColor );
		for( float& c : p.floorLightColor )
			c *= 0.55f;
		p.floorY     = values[ P_POS_Y ] + values[ P_FLOOR_HEIGHT ];
		if( Opt( P_CATCHER_PLANE ) == 1 )
		{
			const float wallZ = values[ P_POS_Z ] - values[ P_WALL_DIST ];
			const float pl[ 4 ] = { 0, 0, 1, -wallZ };
			std::memcpy( p.catchPlane, pl, sizeof( pl ) );
		}
		else
		{
			const float pl[ 4 ] = { 0, 1, 0, -p.floorY };
			std::memcpy( p.catchPlane, pl, sizeof( pl ) );
		}
		p.mirror         = values[ P_MIRROR ];
		p.mirrorBlur     = values[ P_MIRROR_BLUR ];
		p.mirrorFade     = values[ P_MIRROR_FADE ];
		p.sil            = values[ P_SILHOUETTE ];
		p.silFill        = values[ P_SIL_FILL ];
		p.silGlow        = values[ P_SIL_GLOW ] * Alpha( P_SIL_COL_H );
		p.silWidth       = values[ P_SIL_WIDTH ];
		Colour( P_SIL_COL_H, p.silColor );
		p.bloom          = values[ P_BLOOM ];
		p.bloomThreshold = values[ P_BLOOM_THRESHOLD ];
		p.bloomRadius    = values[ P_BLOOM_RADIUS ];
		p.ghosts         = values[ P_FLARE_GHOSTS ];
		p.streak         = values[ P_FLARE_STREAK ];
	}

	// ---- sweeping spots: a rig in front of the mask, each spot aiming at a moving point on the mask
	int BuildSpots( FrameState& fs, int n, const float* center, float pulse, float dt )
	{
		const int count = std::min( 4, std::max( 0, Opt( P_SPOT_COUNT ) ) );
		// Spot Beat Strobe steps the hue a quarter turn x strobe on every beat; at 0 the offset glides home like a Hue Drift
		const bool beat = beatCount != lastSpotBeat;
		lastSpotBeat    = beatCount;
		Drift( 3, 0.0f, dt, beat ? 0.25 * values[ P_SPOT_STROBE ] : 0.0, values[ P_SPOT_STROBE ] > 0.0f );
		// sweeps (the param is sweeps/min). Random reads a copy at x3 and x2.5 of Smooth1D, so that copy wraps at 400 pi
		const double sweeps = dt * values[ P_SPOT_SPEED ] / 60.0;
		clk.spot            = Wrap( clk.spot + sweeps, 1.0 );
		clk.spotWander      = Wrap( clk.spotWander + sweeps, 2.0 * kSmoothPeriod );
		const float spotPhase = float( clk.spot );
		const float width = values[ P_SPOT_WIDTH ], strobe = values[ P_SPOT_STROBE ];
		const int pattern = Opt( P_SPOT_PATTERN ), mode = Opt( P_SPOT_COLOR_MODE );
		for( int i = 0; i < count && n < kMaxXLights; ++i )
		{
			XLight& x   = fs.xl[ n++ ];
			x.type      = 0;
			x.shadow    = false;
			x.coneDeg   = values[ P_SPOT_CONE ];
			x.coneSoft  = 0.3f;
			x.pos[ 0 ]  = center[ 0 ] + ( float( i ) - 0.5f * float( count - 1 ) ) * 1.6f;
			x.pos[ 1 ]  = values[ P_SPOT_HEIGHT ];
			x.pos[ 2 ]  = center[ 2 ] + values[ P_SPOT_DIST ];
			const float ph = spotPhase * 6.2831853f + float( i ) * 6.2831853f / float( std::max( count, 1 ) ) * 0.5f;
			float tx = 0, ty = 0;
			switch( pattern )
			{
			case 0: tx = std::sin( ph ); break;
			case 1: tx = std::sin( ph ), ty = std::cos( ph ); break;
			case 2: tx = std::sin( ph ), ty = 0.5f * std::sin( 2.0f * ph ); break;
			case 3: tx = std::sin( spotPhase * 6.2831853f ) * ( i % 2 ? -1.0f : 1.0f ), ty = 0.3f * std::cos( spotPhase * 6.2831853f ); break;
			default: // wander: the fastest wiggle is ~2 per sweep, so Random is no busier than the other patterns
				tx = Smooth1D( clk.spotWander * 3.0, float( i ) * 5.1f ), ty = 0.7f * Smooth1D( clk.spotWander * 2.5, float( i ) * 7.3f + 2.0f );
				break;
			}
			float d[ 3 ] = { center[ 0 ] + tx * width - x.pos[ 0 ], center[ 1 ] + ty * width - x.pos[ 1 ], center[ 2 ] - x.pos[ 2 ] };
			const float len = std::max( 1e-5f, std::sqrt( d[ 0 ] * d[ 0 ] + d[ 1 ] * d[ 1 ] + d[ 2 ] * d[ 2 ] ) );
			for( int k = 0; k < 3; ++k )
				x.dir[ k ] = d[ k ] / len;
			if( i == 0 && LogVerbose() )
			{
				// Debug Log: how fast spot 1 turns (degrees per second, and the largest step in one frame)
				const float c = std::min( 1.0f, spotPrevDir[ 0 ] * x.dir[ 0 ] + spotPrevDir[ 1 ] * x.dir[ 1 ] + spotPrevDir[ 2 ] * x.dir[ 2 ] );
				const float a = std::acos( std::max( -1.0f, c ) ) / kDeg;
				if( spotLogClock >= 0.0f )
					spotLogSum += a, spotLogMax = std::max( spotLogMax, a );
				std::memcpy( spotPrevDir, x.dir, sizeof( spotPrevDir ) );
				spotLogClock += dt;
				if( spotLogClock >= 1.0f )
				{
					Log( "SPOTS Speed %.2f sweeps/min: spot 1 aim turned %.1f deg in %.2f s (max %.2f deg per frame)", values[ P_SPOT_SPEED ], spotLogSum, spotLogClock, spotLogMax );
					spotLogClock = spotLogSum = spotLogMax = 0.0f;
				}
			}
			float hue = mode == 1 ? float( i ) / float( std::max( count, 1 ) ) : ( mode == 2 && ( i % 2 ) ? 0.5f : 0.0f );
			hue += float( clk.lightHue[ 3 ] ); // colour steps on every beat (Spot Beat Strobe), home at strobe 0
			float c[ 3 ];
			if( mode == 1 ) // rainbow ignores the picked hue and saturation: saturated colours round the wheel
				HsbToRgb( hue, 0.9f, values[ P_SPOT_COL_V ], c );
			else
				ColourDrift( P_SPOT_COL_H, hue, c );
			const float gain = values[ P_SPOT_INTENSITY ] * Alpha( P_SPOT_COL_H ) * ( 1.0f + 2.0f * strobe * pulse );
			for( int k = 0; k < 3; ++k )
				x.color[ k ] = std::max( 0.0f, c[ k ] ) * gain;
		}
		return n;
	}

	// Every integrated phase, in double, each wrapped to a period its consumers are seamless across (W14: no jumps
	// ever, however long the show runs). Whatever reaches a float uniform is small enough to keep its precision.
	struct Clocks
	{
		double spin[ 3 ] = {}, lowPolySpin[ 3 ] = {};      // degrees
		double tumble[ 3 ] = {}, tumbleSway[ 3 ] = {};     // degrees: steady turn, rocking phase
		double wobble = 0, shake = 0, spotWander = 0;      // Smooth1D arguments
		double lightHue[ 4 ] = {}, spot = 0, cloneOrbit = 0; // turns (hue offsets: A, B, C, spots)
		double trailGrain[ 2 ] = {};                         // Trail Dust grain drift, cells (the grain hash repeats every 256)
		double smoke[ 3 ] = {}, vol = 0;                   // noise units; seconds (motes)
		double surf = 0, holo = 0, glitter = 0, chrome[ 2 ] = {}, film = 0, pattern = 0;
		double slice = 0, dustCurl = 0, ripple = 0, contour = 0;
		double face = 0; // seconds; only differences are used, never wrapped
		double typeOn = 0, letterWave = 0, letterSpin = 0; // W21: type cycle (fraction, wraps = the restart), wave cycles, degrees
		double palette = 0, kaleido = 0, fractal = 0, band = 0, ghost = 0; // W24 Trip: turns, degrees, degrees, bands, ghost spawn phase
		double fire = 0;                                                     // W26 fire flicker: Smooth1D arg (x9, x23)
		double liquid = 0;                                                   // W26 liquid wobble: cycles
		double strike = 0;                                                   // W26 Strike Rate phase (wraps at 1 = a strike)
		double swarm = 0, swarmBlink = 0;                                    // W26 swarm: wander drift (64), seconds (kSwarmLoop)
		double lavaFlow = 0, lavaHaze = 0;                                   // W26 lava: flow drift (64), haze cycles (1)
		double caustic = 0;                                                  // W26 caustic ripple drift (64)
		double transPlay = 0; // W28 Transition Play, seconds (Once: held at Transition Time; Ping-Pong: wraps at 2 x; Loop: 1 x + hold)
		double video = 0;            // W2 projector video: seconds (Loop: wraps at the clip length, Ping-Pong at twice it; Once clamps)
		double warp = 0, glitch = 0; // W2 Deform: Noise Warp drift (lattice cells, wraps at 256), Glitch steps (wraps at 1000)
	} clk;
	// W2 Jelly: a damped spring per instance (fitted / mask space), driven by the mask's own acceleration (move, spin, bounce)
	struct Jelly
	{
		float lin[ 3 ] = {}, linV[ 3 ] = {}, ang[ 3 ] = {}, angV[ 3 ] = {}, scale = 0, scaleV = 0;
		float prevPos[ 3 ] = {}, prevVel[ 3 ] = {}, prevW[ 3 ] = {}, prevScale = 1, prevScaleV = 0, prevR[ 9 ] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		int seen = 0; // frames of history (the first two only fill it: no kick from nothing)
	} jelly;
	// W28 trigger watch + Transition Play
	uint64_t frames = 0, lastJumpLog = 0;
	int msVotes = 0;        // W28: > 0 = the host clock is in milliseconds (Arena 7.28)
	double hostScale = 1.0;
	unsigned setCalls = 0, setChanged = 0;
	bool transRestart = true;
	int lastTransPlay = -1;
	bool crackAuto = false; // a trigger's lava spread, waiting for the model
	// W26
	std::shared_ptr< SdfJob > wowSdf;
	const CpuGeometry* wowSdfFor = nullptr;
	uint32_t lastBurstBeat = 0, lastStrikeBeat = 0, strikeCount = 0;
	bool strikeRequest = false;
	bool swarmRelease = false;
	int crackPress = 0;
	bool crackManual = true;
	float crackProgress = 0, crackTarget = 0, crackSeed[ 3 ] = { 0, 0, 0.9f }, flareT = 10.0f;
	uint32_t crackStarts = 0, lastCrackBeat = 0;
	uint32_t lastReleaseBeat = 0;
	float releaseT = 1000.0f;
	int lastSwarmCount = 0;
	Strike strikes[ 4 ];
	int strikeNext = 0;
	float burstT = 10.0f;
	std::string projImageFile;
	std::shared_ptr< VideoPlayer > video; // W2: Proj File is a video
	bool videoRestart = true, videoLogged = false;
	float videoCue = -1.0f;        // W4: a user's Video Position change waiting to cue (-1 = none)
	int videoCueHold = -1;         // the cued frame the clock waits on until it is shown (-1 = none)
	float videoCueWait = 0.0f;
	int videoScrubLast = 0;        // the frame shown last (the scrub direction)
	bool videoScrubRefused = false; // W6: Scrub picked on a codec that cannot scrub (plays as Loop; Video Time says so)
	float videoPushT = 0.0f, videoTimeT = 1.0f;
	float videoPushed[ 8 ] = { -1, -1, -1, -1, -1, -1, -1, -1 }; // the last pushed playhead values (echo filter)
	unsigned videoPushN = 0;
	uint64_t videoLateLogged = 0;
	float videoStatT = 0.0f;
	std::shared_ptr< struct ProjDecode > projImageJob;
	std::shared_ptr< const ProjImage > projImage;
	uint64_t projImageSerial = 0;
	float hopT = 1000.0f;
	uint32_t lastHopBeat = 0;
	std::vector< float > values;
	float driftReturnT[ 4 ] = { -1, -1, -1, -1 }; // seconds into each hue offset's glide home (-1 = not gliding)
	uint32_t lastSpotBeat   = 0;
	float spotPrevDir[ 3 ] = { 0, 0, 1 }, spotLogClock = -1.0f, spotLogSum = 0, spotLogMax = 0; // Debug Log only
#if LOADEHR_FX
	FaceTracker faceTracker;
	std::vector< FaceTrack > faceTracks;
	float noFaceAlpha = 1;
	double lastVisionMs = 0;
	uint64_t visionResults = 0;
#endif
	// Stage 6 camera live
	float poses[ 4 ][ 9 ] = {};
	std::string poseData;
	std::string eyePlacedFor; // hidden "C Placed For": the model the eye lights were placed for
	float glideFrom[ 9 ] = {}, glideTo[ 9 ] = {}, glideNow[ 9 ] = {}, glideT = 0;
	bool glideActive = false;
	int cutIndex = 0;
	uint32_t lastCutBeat = 0, lastShakeBeat = 0, cutRng = 12345u;
	float shakeEnv = 0;
	float cloneLogDeg = 0, cloneLogClock = 0; // Debug Log accumulators
	static constexpr int kPoseHistory = 256;
	Mat4 poseHistory[ kPoseHistory ];
	int poseHead = 0, poseCount = 0;
	float beatClock = 0, beatPeriod = 0.5f;
	float shatterT = 0, shatterClock = 0, reformFrom = 0, reformClock = 0, autoReformAt = -1;
	int shatterState = 0, shatterRequest = 0; // state 0 whole, 1 flying, 2 reforming; request 1 shatter, 2 reform
	uint32_t shatterGen = 0;
	uint32_t lastShatterBeat = 0;
	float dustTarget = 0, dustProgress = 0, dustReturnTimer = 0, dustClock = 0;
	int dustPress   = 0;     // 1 Blow To Dust, 2 Dust Reform (read on the next frame)
	bool dustManual = true;  // the Dust slider drives the cut (until a press)
	bool dustAuto   = false; // this blow came from Dust On Beat
	uint32_t lastDustBeat = 0, lastFlipBeat = 0;
	// W20 smoke sim
	float simPrevModel[ 16 ] = {};
	bool simPrevValid = false, simResetRequest = false;
	uint32_t lastPuffBeat = 0;
	float puffT           = 10.0f;
	uint32_t lastRingBeat = 0;                // W24 Trip
	struct Ring
	{
		float t = 10.0f; // seconds since it fired
		float pos[ 3 ] = {}, dir[ 3 ] = { 0, 0, 1 };
	} rings[ 2 ];                             // W25: newest first
	bool ringFire = false;
	float ghostAge[ 3 ]   = { -1, -1, -1 };   // seconds into each ghost's peel (-1 = none)
	float flipBase = -90, flipDeg = 90, flipT = 1000, flipDur = 0.3f, tumbleResetT = -1;
	uint32_t beatCount    = 0;
	float lastBeatPhase   = 0;
	EyeRequest eyeRequest = EyeNone;
	std::shared_ptr< EyeJob > eyeJob;
	std::string eyeJobKey;
	bool eyeJobLogged = false;
	// W18 Fit To: the fit in use (centre xyz, scale) glides to the target when parts are shown / hidden; one per model slot
	float fitNow[ kModelSlots ][ 4 ] = { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } };
	float fitFrom[ kModelSlots ][ 4 ] = { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } };
	float fitTo[ kModelSlots ][ 4 ] = { { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } };
	float fitT[ kModelSlots ] = { 1, 1, 1, 1 };
	uint64_t fitSerial[ kModelSlots ] = {};
	ModelSlot slots[ kModelSlots ];
	std::string modelParam[ kModelSlots ];   // the Model A..D FILE params (slot A shows Model A when Model Source = Model File)
	std::string status[ kModelSlots ];       // the Status A..D lines
	bool reloadRequested[ kModelSlots ] = {};
	int domSlot = -1;                        // the chain's dominant model this frame (-1: nothing loaded)
	Mat4 domM, slotM[ kModelSlots ];         // its model matrix; every slot's (this frame)
	uint64_t partNamesFor = ~0ull;           // the model the Parts group is labelled after (0 = none)
	int simDomSlot = -1;                     // the slot the sim obstacle was last (a change = a new obstacle, no velocity jump)
	std::map< unsigned, std::string > texts; // W21 text / file params (Text, Font Name, Font File, Logo File)
	std::vector< std::string > styles;       // Font Style's faces (element i + 1)
	std::string display;
	std::chrono::steady_clock::time_point lastClock;
	Renderer renderer;
	float audio[ 3 ]   = { 0, 0, 0 };
	double lastHostTime = 0;
	bool hostTimeSeen = false, hostTimeValid = false;
	std::vector< float > perfIv, perfCpu; // W29: this second's frame intervals and ProcessOpenGL wall ms
	std::chrono::steady_clock::time_point perfStart;
};

extern "C" __attribute__( ( visibility( "default" ) ) ) void LoadEhrTestAdvance( void* instance, double seconds, int fromZero )
{
	if( instance )
		static_cast< LoadEhrPlugin* >( static_cast< CFFGLPlugin* >( instance ) )->FastForward( seconds, fromZero != 0 );
}

namespace
{
void DeinitBundle()
{
	ShutdownEyeFinder();
	ShutdownSdfBuilder();
	ShutdownLoaders();
}

#if LOADEHR_WOW && LOADEHR_FX
CFFGLPluginInfo PluginInfo( PluginFactory< LoadEhrPlugin >, "LEwf", "LoadEhr Wow FX", 2, 3, 1, 0, FF_EFFECT,
                            "LoadEhr with the W26 showpieces (projector + video, fire, lightning, swarm, lava, caustics, trip, ...); the incoming layer can be background, skin or reflection.",
                            "LoadEhr by Amrit Rosell", 0, nullptr, nullptr, DeinitBundle );
#elif LOADEHR_WOW
CFFGLPluginInfo PluginInfo( PluginFactory< LoadEhrPlugin >, "LEwo", "LoadEhr Wow", 2, 3, 1, 0, FF_SOURCE,
                            "LoadEhr with the W26 showpieces (projector + video, fire, lightning, swarm, lava, caustics, trip, ...) over a transparent background.",
                            "LoadEhr by Amrit Rosell", 0, nullptr, nullptr, DeinitBundle );
#elif LOADEHR_FX
CFFGLPluginInfo PluginInfo( PluginFactory< LoadEhrPlugin >, "LEfx", "LoadEhr FX", 2, 3, 1, 0, FF_EFFECT,
                            "Load any 3D model (fbx/obj/gltf/glb) and animate it; the incoming layer can be background, skin or reflection.",
                            "LoadEhr by Amrit Rosell", 0, nullptr, nullptr, DeinitBundle );
#else
CFFGLPluginInfo PluginInfo( PluginFactory< LoadEhrPlugin >, "LEhr", "LoadEhr", 2, 3, 1, 0, FF_SOURCE,
                            "Load any 3D model (fbx/obj/gltf/glb) and animate it over a transparent background.",
                            "LoadEhr by Amrit Rosell", 0, nullptr, nullptr, DeinitBundle );
#endif
}
