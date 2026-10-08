#include "LoadJob.h"
#include "Generate.h"
#include "Log.h"
#include "ModelLoader.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <thread>

namespace mehr
{
namespace
{
std::mutex g_jobsMutex;
std::map< std::string, std::weak_ptr< LoadTicket > > g_jobs;
std::atomic< int > g_liveWorkers{ 0 };
}

std::string ModelKey( const std::string& path, int maxTexture )
{
	if( IsGenSpec( path ) )
		return GenSpecKey( path );
	struct stat st;
	if( path.empty() || stat( path.c_str(), &st ) != 0 )
		return {};
	return path + "|" + std::to_string( (long long)st.st_mtimespec.tv_sec ) + "." + std::to_string( (long long)st.st_mtimespec.tv_nsec ) + "|" + std::to_string( (long long)st.st_size ) + "@" + std::to_string( maxTexture );
}

std::shared_ptr< LoadTicket > RequestLoad( const std::string& path, int maxTexture, bool forceFresh )
{
	std::string key = ModelKey( path, maxTexture );
	std::lock_guard< std::mutex > lock( g_jobsMutex );
	if( !forceFresh && !key.empty() )
	{
		auto it = g_jobs.find( key );
		if( it != g_jobs.end() )
			if( auto existing = it->second.lock() )
				return existing;
	}
	auto ticket  = std::make_shared< LoadTicket >();
	ticket->path = path;
	ticket->key  = key;
	ticket->maxTexture = maxTexture;
	if( !key.empty() )
		g_jobs[ key ] = ticket;

	g_liveWorkers++;
	std::thread( [ ticket ]() {
		auto t0 = std::chrono::steady_clock::now();
		std::string error;
		std::unique_ptr< ModelData > model;
		try
		{
			if( IsGenSpec( ticket->path ) )
				model = GenerateModel( ticket->path, ticket->cancel, error, ticket->status );
			else
				model = LoadModel( ticket->path, ticket->maxTexture, ticket->cancel, error );
		}
		catch( const std::exception& e )
		{
			error = std::string( "exception: " ) + e.what();
			Log( "LOAD FAIL %s: %s", ticket->path.c_str(), error.c_str() );
		}
		catch( ... )
		{
			error = "unknown exception";
			Log( "LOAD FAIL %s: %s", ticket->path.c_str(), error.c_str() );
		}
		if( model )
		{
			auto g = std::make_shared< CpuGeometry >();
			g->pos.resize( model->vertices.size() * 3 );
			g->uv.resize( model->vertices.size() * 2 );
			for( size_t i = 0; i < model->vertices.size(); ++i )
			{
				const Vertex& v = model->vertices[ i ];
				g->pos[ i * 3 ] = v.px, g->pos[ i * 3 + 1 ] = v.py, g->pos[ i * 3 + 2 ] = v.pz;
				g->uv[ i * 2 ] = v.u, g->uv[ i * 2 + 1 ] = v.v;
			}
			// W2: a text / logo with its fractured copy: the geometry jobs see the whole mesh only (the copy is drawn, not sampled)
			const size_t whole = model->wholeIndexCount ? model->wholeIndexCount : model->indices.size();
			g->idx.assign( model->indices.begin(), model->indices.begin() + whole );
			g->fracIdx.assign( model->indices.begin() + whole, model->indices.end() );
			g->fracPiece  = model->triPiece;
			g->pieceCount = model->pieceCount;
			for( const Part& p : model->parts )
			{
				if( p.fractured )
					continue;
				g->parts.push_back( { p.firstIndex, p.indexCount } );
				g->partSlot.push_back( p.slot );
			}
			g->triGroup   = model->triGroup;
			g->groupCount = model->groupCount;
			for( int k = 0; k < 3; ++k )
				g->fitCenter[ k ] = model->fitCenter[ k ];
			g->fitScale     = model->fitScale;
			if( !g->triGroup.empty() )
			{
				g->groupBox.assign( size_t( g->groupCount ) * 6, 0.0f );
				for( int k = 0; k < g->groupCount; ++k )
					for( int a = 0; a < 3; ++a )
						g->groupBox[ k * 6 + a ] = 1e30f, g->groupBox[ k * 6 + 3 + a ] = -1e30f;
				for( size_t i = 0; i < g->idx.size(); ++i )
				{
					float* b       = &g->groupBox[ size_t( g->triGroup[ i / 3 ] ) * 6 ];
					const float* p = &g->pos[ size_t( g->idx[ i ] ) * 3 ];
					for( int a = 0; a < 3; ++a )
					{
						const float f = ( p[ a ] - g->fitCenter[ a ] ) * g->fitScale;
						b[ a ] = std::min( b[ a ], f ), b[ 3 + a ] = std::max( b[ 3 + a ], f );
					}
				}
			}
			model->geometry = std::move( g );
		}
		ticket->model   = std::move( model );
		ticket->error   = error;
		ticket->msTotal = std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - t0 ).count();
		ticket->done.store( true );
		g_liveWorkers--;
	} ).detach();
	return ticket;
}

void ShutdownLoaders()
{
	{
		std::lock_guard< std::mutex > lock( g_jobsMutex );
		for( auto& kv : g_jobs )
			if( auto t = kv.second.lock() )
				t->cancel.store( true );
		g_jobs.clear();
	}
	for( int i = 0; i < 500 && g_liveWorkers.load() > 0; ++i )
		std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
}
}
