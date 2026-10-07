// W26 Stage 10: the infinity mirror room (MirrorShaders.h). The mask's mirror images are instanced MIRROR variants of the
// main programs (batches of 64, LOD by distance); the LED edges are one full-screen pass after the composite.
// W27: the copies are shaded where the mask really is (lights, shadows, env), and everything else reflects too: the volumes
// march through the lattice, particles / bolts / beams are drawn per mirror cell (the MIRROR variants live next to their
// plain programs: Volume, Sim, Projector, Swarm, Lightning, Crack, Shape).
#include "MathUtil.h"
#include "MirrorShaders.h"
#include "Renderer.h"
#include "Shaders.h"
#include "VolumeShaders.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mehr
{
namespace
{
inline GLint U( GLuint p, const char* n ) { return glGetUniformLocation( p, n ); }
}

bool Renderer::InitMirror( std::string& error )
{
	// W27: the VS folds each copy back to where the mask really is (shading), the FS sees it from the folded camera
	const std::string vs = "#define INSTANCED\n#define MIRROR\n";
	const std::string fs = vs + kLightLib + kMainFS;
	progMainM     = Link( vs + kMainVS, fs, error );
	progSoupM     = Link( vs + kSoupVS, fs, error );
	progMirrorLed = Link( kQuadVS, std::string( kToneLib ) + kMirrorLedFS, error );
	if( !progMainM || !progSoupM || !progMirrorLed )
		return false;
	for( GLuint p : { progMainM, progSoupM } )
	{
		glUseProgram( p );
		char name[ 16 ];
		for( int i = 0; i < CH_COUNT; ++i )
		{
			snprintf( name, sizeof( name ), "uTex%d", i );
			glUniform1i( U( p, name ), i );
		}
		glUniform1i( U( p, "uInputTex" ), 8 );
		glUniform1i( U( p, "uIdxBuf" ), 9 );
		glUniform1i( U( p, "uVtxBuf" ), 10 );
		BindShadowSamplers( p );
	}
	glUseProgram( progMirrorLed );
	glUniform1i( U( progMirrorLed, "uDepth" ), 0 );
	glUseProgram( 0 );
	return true;
}

void Renderer::ReleaseMirror()
{
	for( GLuint* p : { &progMainM, &progSoupM, &progMirrorLed } )
		if( *p )
			glDeleteProgram( *p ), *p = 0;
}

// the copies through p (a MIRROR program, bound, its own pass's uniforms set): batches of 64 instances
void Renderer::DrawMirrorPass( GLuint p, const FrameState& fs, GpuModel* model, bool soup )
{
	const MirrorState& m = fs.mirror;
	glUniform3fv( U( p, "uMirrorTint" ), 1, m.tint );
	glUniform3fv( U( p, "uMirrorBoxC" ), 1, m.center );
	glUniform3fv( U( p, "uMirrorBoxH" ), 1, m.half );
	glUniform3fv( U( p, "uMirrorEye" ), 1, fs.camPos );
	for( size_t i = 0; i < m.copies.size(); i += 64 )
	{
		const std::vector< Instance > batch( m.copies.begin() + i, m.copies.begin() + std::min( m.copies.size(), i + 64 ) );
		DrawModel( p, fs, model, false, soup, 0, nullptr, nullptr, &batch );
	}
}

void Renderer::DrawMirrorCopies( const FrameState& fs, GpuModel* model )
{
	const MirrorState& m = fs.mirror;
	if( m.shape == 0 || m.copies.empty() || !model )
		return;
	const bool soup   = fs.wireMix > 0.0f || fs.explode > 0.0f || fs.shape.on;
	const bool bounce = soup && fs.shape.bounce && fs.shape.shatterT > 0.0f && fs.shape.transSide == 0; // W27: the recorded flights
	const GLuint p    = bounce ? progSoupBM : soup ? progSoupM : progMainM;
	glUseProgram( p );
	DrawMirrorPass( p, fs, model, soup );
}

// ---- W27: everything else in the room. Volumes march through the lattice (kMirrorMarchLib); particles and bolts are
// drawn once per cell (kMirrorPointLib), the nearest Particle Images cells.
GLuint Renderer::MirrorVariant( GLuint p, const FrameState& fs ) const
{
	const MirrorState& m = fs.mirror;
	if( m.shape == 0 )
		return p;
	if( m.volumes )
	{
		const GLuint vol[][ 2 ] = { { progFogMarch, progFogMarchM }, { progFogMarchT, progFogMarchTM }, { progFogMarchSim, progFogMarchSimM },
		                            { progFogMarchSimT, progFogMarchSimTM }, { progFogMarchSimF, progFogMarchSimFM }, { progProjMarch, progProjMarchM },
		                            { progProjMarchSim, progProjMarchSimM } };
		for( const auto& v : vol )
			if( p == v[ 0 ] )
				return v[ 1 ];
	}
	return p;
}

int Renderer::MirrorImages( const FrameState& fs ) const
{
	const MirrorState& m = fs.mirror;
	return m.shape > 0 && m.particles ? std::min( m.images, (int)m.cells.size() ) : 0;
}

void Renderer::SetMirrorUniforms( GLuint p, const FrameState& fs )
{
	const MirrorState& m = fs.mirror;
	glUniform3fv( U( p, "uMBoxC" ), 1, m.center );
	glUniform3fv( U( p, "uMBoxH" ), 1, m.half );
	glUniform3fv( U( p, "uMEye" ), 1, fs.camPos );
	glUniform3fv( U( p, "uMAxes" ), 1, m.axes );
	glUniform3fv( U( p, "uMTint" ), 1, m.tint );
	glUniform1i( U( p, "uMBounces" ), m.bounces );
	glUniform1i( U( p, "uMSteps" ), std::min( 256, m.volSteps * m.bounces ) );
}

// cell i of the particle images: its index, tint^bounces; eye (optional) = the camera folded into the box, for passes that light
// a particle where it really is (uCamPos)
void Renderer::SetMirrorCell( GLuint p, const FrameState& fs, int i, const float* eye )
{
	const MirrorState& m = fs.mirror;
	const MirrorCell& c  = m.cells[ size_t( i ) ];
	glUniform4f( U( p, "uMCell" ), c.index[ 0 ], c.index[ 1 ], c.index[ 2 ], c.bounces );
	float tint[ 3 ];
	for( int k = 0; k < 3; ++k )
		tint[ k ] = std::pow( m.tint[ k ], c.bounces );
	glUniform3fv( U( p, "uMTint" ), 1, tint );
	if( eye )
	{
		float f[ 3 ];
		for( int k = 0; k < 3; ++k )
		{
			const float s = std::fmod( std::fabs( c.index[ k ] ), 2.0f ) > 0.5f ? -1.0f : 1.0f;
			f[ k ]        = s * ( eye[ k ] - m.center[ k ] - 2.0f * m.half[ k ] * c.index[ k ] ) + m.center[ k ];
		}
		glUniform3fv( U( p, "uCamPos" ), 1, f );
	}
}

// model-swap prewarm (WarmWow's 1-pixel scissor; t = its state: projector, bolts set up): every MIRROR program once
void Renderer::WarmMirror( const FrameState& fs, GpuModel* model, int w, int h )
{
	FrameState t     = fs;
	MirrorState& m   = t.mirror;
	m.shape          = 1;
	m.volumes        = m.particles = true;
	m.images         = 1;
	m.cells.assign( 1, MirrorCell() );
	m.cells[ 0 ].index[ 0 ] = m.cells[ 0 ].bounces = 1.0f;
	if( model )
	{
		m.copies.assign( 1, Instance() );
		std::memcpy( m.copies[ 0 ].m, fs.model, sizeof( fs.model ) );
		glBindFramebuffer( GL_FRAMEBUFFER, tSamples > 1 ? msFBO : resolveFBO );
		glViewport( 0, 0, w, h );
		glEnable( GL_DEPTH_TEST );
		DrawMirrorCopies( t, model );
		t.shape.on = true;
		DrawMirrorCopies( t, model );
		t.shape.bounce = true, t.shape.shatterT = 0.1f, t.shape.transSide = 0; // the recorded flights
		DrawMirrorCopies( t, model );
		t.shape = fs.shape;
		// the copies' light passes: the projector / caustic picture, the lava glow
		glEnable( GL_BLEND );
		glBlendFuncSeparate( GL_ONE, GL_ONE, GL_ZERO, GL_ONE );
		glDepthFunc( GL_LEQUAL );
		for( GLuint p : { progProjMainM, progProjSoupM, progCrackMainM, progCrackSoupM } )
		{
			glUseProgram( p );
			if( p == progProjMainM || p == progProjSoupM )
				BindCookieLights( p, t );
			DrawMirrorPass( p, t, model, p == progProjSoupM || p == progCrackSoupM );
		}
		glDepthFunc( GL_LESS );
		glDisable( GL_BLEND );
		glDisable( GL_DEPTH_TEST );
		glBindFramebuffer( GL_FRAMEBUFFER, resolveFBO );
		DrawMirrorLeds( t, w, h );
	}
	// the projector's beam through the lattice, the bolts and fireflies per cell
	glBindFramebuffer( GL_FRAMEBUFFER, resolveFBO );
	glViewport( 0, 0, w, h );
	const bool keep = projOk;
	projOk          = true;
	DrawProjMarch( t, false );
	DrawProjMarch( t, true );
	projOk = keep;
	glBindFramebuffer( GL_FRAMEBUFFER, resolveFBO );
	RenderGlow( t, w, h );
	glBindFramebuffer( GL_FRAMEBUFFER, resolveFBO );
	glEnable( GL_SCISSOR_TEST );
}

void Renderer::DrawMirrorLeds( const FrameState& fs, int w, int h )
{
	const MirrorState& m = fs.mirror;
	if( m.ledGlow <= 0.0f )
		return;
	Mat4 V, P;
	std::memcpy( V.m, fs.view, sizeof( V.m ) );
	std::memcpy( P.m, fs.proj, sizeof( P.m ) );
	const Mat4 inv = ( P * V ).Inverse();
	glUseProgram( progMirrorLed );
	glUniformMatrix4fv( U( progMirrorLed, "uInvViewProj" ), 1, GL_FALSE, inv.m );
	glUniform3fv( U( progMirrorLed, "uCamPos" ), 1, fs.camPos );
	glUniform3fv( U( progMirrorLed, "uBoxC" ), 1, m.center );
	glUniform3fv( U( progMirrorLed, "uBoxH" ), 1, m.half );
	glUniform1i( U( progMirrorLed, "uShape" ), m.shape );
	glUniform1i( U( progMirrorLed, "uBounces" ), m.bounces );
	glUniform3fv( U( progMirrorLed, "uTint" ), 1, m.tint );
	float led[ 3 ];
	for( int k = 0; k < 3; ++k )
		led[ k ] = m.led[ k ] * m.ledGlow;
	glUniform3fv( U( progMirrorLed, "uLed" ), 1, led );
	glUniform1i( U( progMirrorLed, "uRainbow" ), m.ledColors );
	glUniform1f( U( progMirrorLed, "uWidth" ), m.ledWidth );
	glUniform1f( U( progMirrorLed, "uPixAng" ), 2.0f / ( std::max( fs.proj[ 5 ], 1e-4f ) * float( std::max( h, 1 ) ) ) );
	glUniform1f( U( progMirrorLed, "uPhase" ), m.phase );
	glUniform1f( U( progMirrorLed, "uExposure" ), fs.exposure );
	glUniform1i( U( progMirrorLed, "uTonemap" ), fs.tonemap );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, sceneDepthTex );
	glEnable( GL_BLEND );
	glBlendFuncSeparate( GL_ONE, GL_ONE, GL_ZERO, GL_ONE );
	glDisable( GL_DEPTH_TEST );
	glBindVertexArray( quadVAO );
	glDrawArrays( GL_TRIANGLES, 0, 3 );
	glBindVertexArray( 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glDisable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ZERO );
	(void)w;
}
}
