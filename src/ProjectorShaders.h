#pragma once
// W26 Stage 2: the projector (a cookie light). Its texture (RGBA32F, the light's own projection) holds the projected picture in
// rgb and the shadow distance in a: the shadow pass writes the distance into all four channels, then kProjCookieFS writes the
// picture into rgb (colour mask RGB). One sampler per cookie light wherever it is read (the main FS has 2 left of 16).
// The beam in the medium: kProjMarchFS, a march along the view ray added onto the fog's march target (rgb only) before the
// temporal blend, so it shares the fog's resolution, jitter, temporal accumulation and composite; the W3 / W20 / W24 march
// programs stay byte-identical. Linked after kMediumLib (Noise) or kSimLib + kMediumSimLib (Sim).

namespace mehr
{
static const char* kProjCookieFS = R"GLSL(
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uImage;
uniform vec2 uImageScale; // host input padded to its hardware size
uniform float uLod;       // Focus: blur by mip level
uniform int uHaveImage;
void main()
{
	vec3 c = uHaveImage == 1 ? textureLod( uImage, vUV * uImageScale, uLod ).rgb : vec3( 1.0 );
	// a soft frame edge (the lens), wider when out of focus
	vec2 e  = min( vUV, 1.0 - vUV );
	float f = smoothstep( 0.0, 0.004 + 0.02 * uLod, min( e.x, e.y ) );
	fragColor = vec4( pow( max( c, 0.0 ), vec3( 2.2 ) ) * f, 0.0 ); // the picture is sRGB-encoded: light is linear
}
)GLSL";

static const char* kProjMarchFS = R"GLSL(
in vec2 vUV;
layout( location = 0 ) out vec4 outFog;
uniform sampler2D uDepth;
uniform sampler2D uPLTex;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform int uSteps;
uniform float uFrame;
uniform mat4 uPLVP;
uniform vec3 uPLPos;
uniform vec3 uPLDir;
uniform vec3 uPLColor; // colour x intensity x Proj Beam
uniform float uPLFar;
float IGN( vec2 p ) { return fract( 52.9829189 * fract( dot( p, vec2( 0.06711056, 0.00583715 ) ) ) ); }
float HG( float c, float g ) { float g2 = g * g; return ( 1.0 - g2 ) / pow( max( 1.0 + g2 - 2.0 * g * c, 1e-4 ), 1.5 ); }
void main()
{
	vec4 f     = uInvViewProj * vec4( vUV * 2.0 - 1.0, 1.0, 1.0 );
	vec3 rd    = normalize( f.xyz / f.w - uCamPos );
	float z    = texture( uDepth, vUV ).r;
	float tEnd = uPLFar;
	if( z < 1.0 )
	{
		vec4 s = uInvViewProj * vec4( vUV * 2.0 - 1.0, z * 2.0 - 1.0, 1.0 );
		tEnd   = min( tEnd, length( s.xyz / s.w - uCamPos ) );
	}
	// only the stretch of the ray inside the projector's frustum: clip space c(t) = A + t B against w +- x, w +- y, w + z
	vec4 A = uPLVP * vec4( uCamPos, 1.0 ), B = uPLVP * vec4( rd, 0.0 );
	float t0 = 0.0, t1 = tEnd;
	float ea[ 5 ] = float[ 5 ]( A.w + A.x, A.w - A.x, A.w + A.y, A.w - A.y, A.w + A.z );
	float eb[ 5 ] = float[ 5 ]( B.w + B.x, B.w - B.x, B.w + B.y, B.w - B.y, B.w + B.z );
	for( int k = 0; k < 5; ++k )
	{
		if( abs( eb[ k ] ) < 1e-7 )
		{
			if( ea[ k ] < 0.0 )
				t1 = -1.0;
			continue;
		}
		float tk = -ea[ k ] / eb[ k ];
		if( eb[ k ] > 0.0 )
			t0 = max( t0, tk );
		else
			t1 = min( t1, tk );
	}
#ifdef MIRROR
	// W27 mirror room: the real ray ends where it leaves the box through a mirror; its reflections run on (folded samples)
	vec2 mr  = MirrorRange( rd, tEnd );
	bool mOn = mr.x < mr.y;
	if( mOn )
		t1 = min( t1, mr.x );
	if( t1 <= t0 && !mOn )
#else
	if( t1 <= t0 )
#endif
	{
		outFog = vec4( 0.0 );
		return;
	}
	float dt = ( t1 - t0 ) / float( uSteps );
	float t  = t0 + dt * IGN( gl_FragCoord.xy + uFrame * vec2( 5.588238, 3.1459 ) );
	vec3 L   = vec3( 0.0 );
	float T  = 1.0;
#ifdef MIRROR
	int nReal = t1 > t0 ? uSteps : 0;
	for( int s = 0; s < nReal + ( mOn ? uMSteps : 0 ); ++s, t += dt )
	{
		if( s == nReal )
		{
			dt = ( mr.y - mr.x ) / float( uMSteps );
			t  = mr.x + dt * IGN( gl_FragCoord.yx + uFrame * vec2( 3.1459, 5.588238 ) );
		}
		vec3 p  = uCamPos + rd * t;
		vec3 wb = vec3( 1.0 ), rdF = rd;
		if( s >= nReal )
		{
			float nb;
			p  = MirrorFold( p, rd, rdF, nb );
			wb = pow( uMTint, vec3( nb ) );
		}
		vec3 rd = rdF;
#else
	for( int s = 0; s < uSteps; ++s, t += dt )
	{
		vec3 p  = uCamPos + rd * t;
#endif
		vec4 c  = uPLVP * vec4( p, 1.0 );
		if( c.w <= 1e-5 )
			continue;
		vec2 uv = c.xy / c.w * 0.5 + 0.5;
		if( any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) )
			continue;
		float smoke;
		float sig = MediumAt( p, smoke ).x;
		if( sig <= 1e-4 )
			continue;
		vec4 ck = textureLod( uPLTex, uv, 0.0 );
		vec3 lv = p - uPLPos;
		float d = length( lv );
		if( d > ck.a + 0.02 + 0.01 * d )
			continue; // behind the mask, seen from the projector
		float ext = exp( -sig * dt );
#ifdef MIRROR
		L += T * wb * uPLColor * ck.rgb * ( HG( dot( lv / max( d, 1e-5 ), -rd ), 0.5 ) / ( 1.0 + d * d ) ) * ( 1.0 - ext );
		T *= 1.0 - dot( wb, vec3( 1.0 / 3.0 ) ) * ( 1.0 - ext );
#else
		L += T * uPLColor * ck.rgb * ( HG( dot( lv / max( d, 1e-5 ), -rd ), 0.5 ) / ( 1.0 + d * d ) ) * ( 1.0 - ext );
		T *= ext;
#endif
	}
	outFog = vec4( L, 0.0 );
}
)GLSL";
}
