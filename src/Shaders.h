#pragma once
// GLSL 4.10 core sources. The main fragment stage is linked with two vertex stages: the indexed
// one (normal drawing) and the triangle-soup one (used only while Wireframe or Explode is non-zero).

namespace mehr
{
static const char* kMainVS = R"GLSL(
layout( location = 0 ) in vec3 aPos;
layout( location = 1 ) in vec2 aUV;
layout( location = 2 ) in vec4 aNormal;
layout( location = 3 ) in vec4 aTangent;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;
uniform vec3 uFitCenter;
uniform float uFitScale;   // raw units -> fitted units (independent of Auto Fit)
uniform float uModelScale; // raw units -> world units along the model matrix (for displacement)
uniform sampler2D uTex4;   // height
uniform int uHas4;
uniform vec4 uSel4;
uniform float uHeightAmt;
uniform float uOutlineWidth; // > 0 only in the outline pass
// instances (clones / ghosts, program variants built with INSTANCED): matrix, hue offset, alpha
uniform int uInstBase;
uniform mat4 uInst[ 64 ];
uniform vec2 uInstHA[ 64 ]; // hue offset, alpha
flat out vec2 vInstHA;
#ifdef INSTANCED
uniform vec4 uInstX[ 64 ]; // keep (dissolve), fog keep, trail age (ghosts), -
flat out vec4 vInstX;
uniform vec3 uTrailDriftW; // ghost pass: where a fully dusty ghost has drifted at the trail's end (world)
uniform float uTrailDustV; // ghost pass: Trail Dust (0 for clones)
vec3 TrailDrift( float age ) { return uTrailDriftW * ( uTrailDustV * smoothstep( 0.05, 0.8, age ) * age ); }
// ghost pass, Trail Smear: vertices facing the way the mask moved slide toward the next newer pose, so each ghost
// stretches over the gap to the one in front of it (one continuous streak, no extra draws)
uniform mat4 uInstNext[ 64 ];
uniform float uTrailSmear;
vec3 TrailSmear( vec3 w, vec3 p, vec3 wn )
{
	vec3 mv = ( uInstNext[ uInstBase + gl_InstanceID ] * vec4( p, 1.0 ) ).xyz - w;
	return dot( wn, mv ) > 0.0 ? mv * uTrailSmear : vec3( 0.0 );
}
#endif
uniform float uGlitchAmt; // hologram glitch on the beat: horizontal slices jump sideways
uniform float uTime;
vec3 Glitch( vec3 w )
{
	float band = floor( w.y * 14.0 ) + floor( uTime * 9.0 ) * 7.13;
	float r    = fract( sin( band * 91.7 ) * 43758.5453 );
	if( r > 0.72 )
		w.x += ( r - 0.86 ) * uGlitchAmt * 0.8;
	return w;
}

out VData
{
	vec3 wpos;
	vec3 wnrm;
	vec4 wtan;
	vec2 uv;
	vec3 opos;
	vec3 bary;
} vo;

void main()
{
	vec3 p = aPos;
	vec3 n = normalize( aNormal.xyz );
	if( uHas4 == 1 && uHeightAmt != 0.0 )
	{
		vec4 hs = textureLod( uTex4, aUV, 1.0 );
		float h = dot( hs, uSel4 );
		p += n * ( h - 0.5 ) * uHeightAmt / max( uModelScale, 1e-6 );
	}
#ifdef INSTANCED
	// clone / ghost: rigid transform + uniform scale, so mat3(M) is a valid normal matrix up to length
	mat4 M   = uInst[ uInstBase + gl_InstanceID ];
	vec4 w   = M * vec4( p, 1.0 );
	vo.wnrm  = normalize( mat3( M ) * n );
	vo.wtan  = vec4( normalize( mat3( M ) * aTangent.xyz ), aTangent.w < 0.0 ? -1.0 : 1.0 );
	vInstHA  = uInstHA[ uInstBase + gl_InstanceID ];
	vInstX   = uInstX[ uInstBase + gl_InstanceID ];
	if( uTrailSmear > 0.0 )
		w.xyz += TrailSmear( w.xyz, p, vo.wnrm );
	w.xyz += TrailDrift( vInstX.z );
#else
	vec4 w   = uModel * vec4( p, 1.0 );
	vo.wnrm  = normalize( uNormalMat * n );
	vo.wtan  = vec4( normalize( mat3( uModel ) * aTangent.xyz ), aTangent.w < 0.0 ? -1.0 : 1.0 );
	vInstHA  = vec2( 0.0, 1.0 );
#endif
	if( uOutlineWidth > 0.0 )
		w.xyz += vo.wnrm * uOutlineWidth;
	if( uGlitchAmt > 0.0 )
		w.xyz = Glitch( w.xyz );
	vo.wpos     = w.xyz;
	vo.uv       = aUV;
	vo.opos     = ( p - uFitCenter ) * uFitScale;
	vo.bary     = vec3( 1.0 );
	gl_Position = uViewProj * w;
}
)GLSL";

// W26 Stage 7: the lava crack network. A jittered 3D lattice in fitted object space (Crack Scale cells per mask unit); the
// crack = the Voronoi border, measured as the true distance to the bisector planes (two passes over 3^3 cells: Quilez).
// Linked only into the CRACKS / CRACK_PASS programs.
static const char* kCrackLib = R"GLSL(
uniform float uCrackScale;
uniform vec3 uCrackSeed;   // fitted: where the cracks start
uniform float uCrackFront; // fitted distance the cracks have spread
vec3 CrackHash( vec3 c ) { return fract( sin( vec3( dot( c, vec3( 127.1, 311.7, 74.7 ) ), dot( c, vec3( 269.5, 183.3, 246.1 ) ), dot( c, vec3( 113.5, 271.9, 124.6 ) ) ) ) * 43758.5453 ); }
// x = distance to the nearest Voronoi border (cell units), yzw = the nearest site (cell units)
vec4 CrackCell( vec3 q )
{
	vec3 i = floor( q ), f = q - i, mr = vec3( 0.0 ), mg = vec3( 0.0 );
	float md = 8.0;
	for( int k = 0; k < 27; ++k )
	{
		vec3 g = vec3( k % 3, ( k / 3 ) % 3, k / 9 ) - 1.0;
		vec3 r = g + 0.1 + 0.8 * CrackHash( i + g ) - f;
		float d = dot( r, r );
		if( d < md )
			md = d, mr = r, mg = g;
	}
	md = 8.0;
	for( int k = 0; k < 27; ++k )
	{
		vec3 g = mg + vec3( k % 3, ( k / 3 ) % 3, k / 9 ) - 1.0;
		vec3 r = g + 0.1 + 0.8 * CrackHash( i + g ) - f;
		if( dot( mr - r, mr - r ) > 1e-5 )
			md = min( md, dot( 0.5 * ( mr + r ), normalize( r - mr ) ) );
	}
	return vec4( md, i + f + mr );
}
)GLSL";

// Triangle-soup vertex stage for Wireframe / Explode: draws glDrawArrays(3 * triangles) and fetches
// the indexed vertices from texture-buffer views of the model's buffers, so each corner knows its
// whole triangle (face normal, centroid, barycentric) without a geometry shader.
static const char* kSoupVS = R"GLSL(
uniform usamplerBuffer uIdxBuf; // R32UI view of the index buffer
uniform usamplerBuffer uVtxBuf; // R32UI view of the vertex buffer, 7 words per vertex
uniform int uFirstIndex;
#ifdef WIRE
uniform usamplerBuffer uEdgeMask; // W5 Feature Edges: per triangle, bit k = draw the edge opposite corner k
uniform int uWireEdges;
flat out vec3 vEdge;
#endif

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;
uniform vec3 uFitCenter;
uniform float uFitScale;
uniform float uModelScale;
uniform sampler2D uTex4;
uniform int uHas4;
uniform vec4 uSel4;
uniform float uHeightAmt;
uniform float uExplode;
uniform float uExplodeRandom;
uniform float uSeed;
uniform float uWorldScale;
// instances (clones / ghosts, program variants built with INSTANCED): matrix, hue offset, alpha
#ifdef DEFORM
uniform int uFoldN; // W2 Mirror Fold: every draw is uFoldN copies, the fold copy = gl_InstanceID % uFoldN
#define INST_ID ( gl_InstanceID / uFoldN )
#else
#define INST_ID gl_InstanceID
#endif
uniform int uInstBase;
uniform mat4 uInst[ 64 ];
uniform vec2 uInstHA[ 64 ]; // hue offset, alpha
flat out vec2 vInstHA;
#ifdef INSTANCED
uniform vec4 uInstX[ 64 ]; // keep (dissolve), fog keep, trail age (ghosts), -
flat out vec4 vInstX;
uniform vec3 uTrailDriftW; // ghost pass: where a fully dusty ghost has drifted at the trail's end (world)
uniform float uTrailDustV; // ghost pass: Trail Dust (0 for clones)
vec3 TrailDrift( float age ) { return uTrailDriftW * ( uTrailDustV * smoothstep( 0.05, 0.8, age ) * age ); }
// ghost pass, Trail Smear: vertices facing the way the mask moved slide toward the next newer pose, so each ghost
// stretches over the gap to the one in front of it (one continuous streak, no extra draws)
uniform mat4 uInstNext[ 64 ];
uniform float uTrailSmear;
vec3 TrailSmear( vec3 w, vec3 p, vec3 wn )
{
	vec3 mv = ( uInstNext[ uInstBase + INST_ID ] * vec4( p, 1.0 ) ).xyz - w;
	return dot( wn, mv ) > 0.0 ? mv * uTrailSmear : vec3( 0.0 );
}
#endif
uniform float uGlitchAmt; // hologram glitch on the beat: horizontal slices jump sideways
uniform float uTime;
vec3 Glitch( vec3 w )
{
	float band = floor( w.y * 14.0 ) + floor( uTime * 9.0 ) * 7.13;
	float r    = fract( sin( band * 91.7 ) * 43758.5453 );
	if( r > 0.72 )
		w.x += ( r - 0.86 ) * uGlitchAmt * 0.8;
	return w;
}

// ---- Stage 4 shape, in FITTED object space (the model spans -1..1 whatever its file units)
uniform int uShapeOn;
uniform usamplerBuffer uShardOf;  // per triangle: shard id
uniform samplerBuffer uShardData; // per shard: centroid (fitted) + seed
uniform int uShardsReady;
uniform float uShatterT;          // flight time in seconds, 0 = assembled
uniform float uShatterForce;
uniform float uShatterGravity;
uniform float uShardSpin;
#ifdef TRANS
uniform int uTransSide; // W16 Shatter transition (its own program): 1 the outgoing model, 2 the incoming one
uniform float uTransE, uTransStagger, uTransEase;
#endif
uniform float uMelt;
uniform float uDrip;
uniform int uSliceAxis;
uniform float uSliceCount;
uniform float uSliceGap;
uniform float uSliceWave;
uniform float uSlide; // transition Slice: every slab slides sideways by a staggered amount
uniform float uVoxel;
uniform float uVoxelSize;
uniform float uLowPoly;
uniform float uLowPolySize;
uniform mat3 uLowPolyGrid;  // mask (fitted) space -> the low-poly grid's space (a rotation)
// facet tumble: one facet per cell of the rotated grid, turning rigidly about the cell's centre
uniform int uTumble;        // 0 off, 1 together, 2 separate
uniform vec3 uTumbleAng;    // the steady turn, degrees (integrated, wrapped at 360)
uniform vec3 uTumblePh;     // the Sway rocking phase, degrees (integrated, wrapped at 360)
uniform float uTumbleRandom;
uniform float uTumbleSway;
uniform float uTumbleW;     // 1 = tumbling, gliding to 0 on Tumble Reset
uniform float uFacetGap;
uniform int uFlipOn;        // beat flips: base angle + one eased kick in progress
uniform float uFlipBase, uFlipT, uFlipDur, uFlipDeg;
uniform int uDeformAxis;
uniform float uTwist;
uniform float uBend;
uniform float uRipple;
uniform float uRippleFreq;
uniform float uRipplePhase;
uniform float uSlicePhase;  // Slice Wave, radians (wrapped)
#ifdef BOUNCE
uniform samplerBuffer uShardPath; // W26: per shard 241 frames x (offset, quaternion), fitted space
#endif
#ifdef CRACKS
uniform float uSplit; // W26 Lava Split (0..1)
#endif
#ifdef LETTERS
// W21 text / logos: every letter moves as one piece (Type On, Wave, Spin, Hop), before every other deform
uniform usamplerBuffer uLetterOf; // per triangle: letter id
uniform samplerBuffer uLetterXf;  // per letter, 3 texels: pivot (fitted) + scale, rotation (quaternion), offset (fitted)
#endif

out VData
{
	vec3 wpos;
	vec3 wnrm;
	vec4 wtan;
	vec2 uv;
	vec3 opos;
	vec3 bary;
} vo;

vec4 UnpackSnorm( uint v )
{
	ivec4 i = ivec4( int( v << 22u ) >> 22, int( v << 12u ) >> 22, int( v << 2u ) >> 22, int( v ) >> 30 );
	return max( vec4( i ) / vec4( 511.0, 511.0, 511.0, 1.0 ), -1.0 );
}
vec3 FetchPos( int v )
{
	int b = v * 7;
	return vec3( uintBitsToFloat( texelFetch( uVtxBuf, b ).r ), uintBitsToFloat( texelFetch( uVtxBuf, b + 1 ).r ), uintBitsToFloat( texelFetch( uVtxBuf, b + 2 ).r ) );
}
vec2 FetchUV( int v )
{
	int b = v * 7;
	return vec2( uintBitsToFloat( texelFetch( uVtxBuf, b + 3 ).r ), uintBitsToFloat( texelFetch( uVtxBuf, b + 4 ).r ) );
}
vec3 FetchNrm( int v ) { return normalize( UnpackSnorm( texelFetch( uVtxBuf, v * 7 + 5 ).r ).xyz ); }
float Hash1( float n ) { return fract( sin( n * 12.9898 + uSeed * 78.233 ) * 43758.5453 ); }
vec3 Hash3v( float n ) { return fract( sin( vec3( n, n + 1.7, n + 3.3 ) * vec3( 12.9898, 78.233, 37.719 ) ) * 43758.5453 ); }

// axis helpers: a = the chosen axis, (u, v) = the other two in cyclic order
vec3 ToAxis( vec3 f, int a ) { return a == 0 ? f.yzx : ( a == 1 ? f.zxy : f ); }
vec3 FromAxis( vec3 q, int a ) { return a == 0 ? q.zxy : ( a == 1 ? q.yzx : q ); }
mat3 RotAxis( vec3 k, float a )
{
	float c = cos( a ), s = sin( a );
	return mat3( c + k.x * k.x * ( 1.0 - c ), k.y * k.x * ( 1.0 - c ) + k.z * s, k.z * k.x * ( 1.0 - c ) - k.y * s,
	             k.x * k.y * ( 1.0 - c ) - k.z * s, c + k.y * k.y * ( 1.0 - c ), k.z * k.y * ( 1.0 - c ) + k.x * s,
	             k.x * k.z * ( 1.0 - c ) + k.y * s, k.y * k.z * ( 1.0 - c ) - k.x * s, c + k.z * k.z * ( 1.0 - c ) );
}
float VNoise( vec2 p )
{
	vec2 i = floor( p ), f = fract( p );
	f      = f * f * ( 3.0 - 2.0 * f );
	float a = fract( sin( dot( i, vec2( 127.1, 311.7 ) ) ) * 43758.5453 ), b = fract( sin( dot( i + vec2( 1, 0 ), vec2( 127.1, 311.7 ) ) ) * 43758.5453 );
	float c = fract( sin( dot( i + vec2( 0, 1 ), vec2( 127.1, 311.7 ) ) ) * 43758.5453 ), d = fract( sin( dot( i + vec2( 1, 1 ), vec2( 127.1, 311.7 ) ) ) * 43758.5453 );
	return mix( mix( a, b, f.x ), mix( c, d, f.x ), f.y );
}

#ifdef DEFORM
// W2 Deform group (this program variant only: with every deform at 0 the plain programs draw). Fitted space; the fixed order:
// Taper, Twist, Bend, Squash, Shear, Bulge, Spherize, Noise Warp, Ripple, Melt / Drip, Glitch, Mirror Fold, Jelly. The values
// arrive already scaled by Deform Amount (+ Audio > Deform).
uniform float uTaper, uSquash, uShear, uBulge, uSpherize;
uniform float uWarp, uWarpScale, uWarpPhase;       // the noise drifts at Warp Speed lattice cells / s (phase wraps at 256)
uniform float uGlitchShift, uGlitchBands, uGlitchStep; // the step index wraps at 1000 (a wrap = one more random step)
uniform vec3 uFoldAxes;                           // 1 = folded across that fitted centre plane
uniform float uFoldK;                             // 0..1: how far folded (Deform Amount)
uniform vec3 uJellyLin, uJellyAng;                // the jelly's lag: offset and rotation (axis x angle) at a fitted radius of 1
uniform float uJellyScale;
vec3 gFoldS = vec3( 1.0 );                        // this copy's side per folded axis (+1 / -1)
bool gFoldFlip = false;                           // this triangle is reflected an odd number of times (winding reversed)
bool gNormalPass = false;                         // the normal's finite differences: no Glitch (a band's jump is a translation)
float DHash( vec3 c )
{
	c = mod( c, 64.0 ); // 64-periodic lattice: the drift wraps seamlessly
	return fract( sin( dot( c, vec3( 127.1, 311.7, 74.7 ) ) ) * 43758.5453 );
}
float DNoise( vec3 p )
{
	vec3 i = floor( p ), f = fract( p );
	f      = f * f * ( 3.0 - 2.0 * f );
	return mix( mix( mix( DHash( i ), DHash( i + vec3( 1, 0, 0 ) ), f.x ), mix( DHash( i + vec3( 0, 1, 0 ) ), DHash( i + vec3( 1, 1, 0 ) ), f.x ), f.y ),
	            mix( mix( DHash( i + vec3( 0, 0, 1 ) ), DHash( i + vec3( 1, 0, 1 ) ), f.x ), mix( DHash( i + vec3( 0, 1, 1 ) ), DHash( i + vec3( 1, 1, 1 ) ), f.x ), f.y ), f.z );
}
vec3 DeformPre( vec3 f, vec3 n )
{
	vec3 q = ToAxis( f, uDeformAxis ); // q.z runs along the axis
	q.xy *= max( 1.0 + uTaper * q.z * 0.5, 0.0 );
	if( uTwist != 0.0 )
	{
		float a = uTwist * q.z, c = cos( a ), s = sin( a );
		q.xy    = vec2( c * q.x - s * q.y, s * q.x + c * q.y );
	}
	if( abs( uBend ) > 1e-4 )
	{
		float R  = 1.0 / uBend;
		float th = uBend * q.z;
		q        = vec3( R - ( R - q.x ) * cos( th ), q.y, ( R - q.x ) * sin( th ) );
	}
	float sa = exp( -uSquash ); // squash: shorter along the axis, wider across it (volume kept)
	q.z *= sa;
	q.xy *= inversesqrt( sa );
	q.x += uShear * q.z * 0.5;
	q.xy *= 1.0 + uBulge * 0.6 * max( 1.0 - q.z * q.z, 0.0 ); // round the middle of the axis out (negative: pinch)
	f = FromAxis( q, uDeformAxis );
	if( uSpherize > 0.0 )
		f = mix( f, normalize( f + vec3( 1e-6 ) ) * 0.8, uSpherize );
	if( uWarp > 0.0 )
	{
		vec3 p = f * uWarpScale + uWarpPhase * vec3( 1.0, 0.5, 0.25 );
		f += uWarp * 0.5 * ( vec3( DNoise( p ), DNoise( p + vec3( 17.3, 9.1, 31.7 ) ), DNoise( p + vec3( 41.9, 23.3, 7.7 ) ) ) - 0.5 );
	}
	if( uRipple > 0.0 )
		f += n * uRipple * sin( uRippleFreq * length( f ) - uRipplePhase );
	if( uMelt > 0.0 )
	{
		float h    = f.y + 1.0;
		float drip = pow( VNoise( f.xz * 7.0 + 3.1 ), 3.0 ) * uDrip * 2.5;
		float sag  = uMelt * h * ( 0.45 + drip ) * ( 0.6 + 0.4 * VNoise( f.xz * 2.3 ) );
		f.y -= sag;
		float floorY = -1.0 - uMelt * 0.15;
		float under  = max( floorY - f.y, 0.0 );
		f.y          = max( f.y, floorY - under * 0.08 );
		f.xz *= 1.0 + uMelt * 0.35 * smoothstep( 0.4, 0.0, f.y - floorY );
	}
	if( uGlitchShift > 0.0 && !gNormalPass )
	{
		// bands across the axis jump sideways on a stepped clock (a hash per band and step: still while the clock is paused)
		vec3 g     = ToAxis( f, uDeformAxis );
		float band = floor( ( g.z + 1.0 ) * 0.5 * uGlitchBands );
		float h    = fract( sin( band * 91.7 + uGlitchStep * 7.13 ) * 43758.5453 );
		float h2   = fract( sin( band * 17.3 + uGlitchStep * 3.71 + 5.0 ) * 24634.6345 );
		if( h > 0.55 )
			g.x += ( h2 - 0.5 ) * uGlitchShift;
		f = FromAxis( g, uDeformAxis );
	}
	return f;
}
vec3 DeformPost( vec3 f )
{
	if( uFoldK > 0.0 ) // kaleidoscope: this copy keeps its own side of each folded centre plane
		f = mix( f, mix( f, gFoldS * abs( f ), uFoldAxes ), uFoldK );
	float w = min( length( f ), 1.5 ); // the jelly: the far parts lag the most
	float a = length( uJellyAng );
	if( a > 1e-6 )
		f = RotAxis( uJellyAng / a, a * w ) * f;
	return f * ( 1.0 + uJellyScale * w ) + uJellyLin * w;
}
#endif
// smooth deforms of one corner (fitted space)
vec3 Continuous( vec3 f, vec3 n )
{
#ifdef DEFORM
	f = DeformPost( DeformPre( f, n ) );
#else
	if( uTwist != 0.0 || uBend != 0.0 )
	{
		vec3 q = ToAxis( f, uDeformAxis ); // q.z runs along the axis
		if( uTwist != 0.0 )
		{
			float a = uTwist * q.z, c = cos( a ), s = sin( a );
			q.xy    = vec2( c * q.x - s * q.y, s * q.x + c * q.y );
		}
		if( abs( uBend ) > 1e-4 )
		{
			// Barr bend of the axis toward q.x, radius 1/bend
			float R  = 1.0 / uBend;
			float th = uBend * q.z;
			q        = vec3( R - ( R - q.x ) * cos( th ), q.y, ( R - q.x ) * sin( th ) );
		}
		f = FromAxis( q, uDeformAxis );
	}
	if( uRipple > 0.0 )
		f += n * uRipple * sin( uRippleFreq * length( f ) - uRipplePhase );
	if( uMelt > 0.0 )
	{
		float h    = f.y + 1.0;                                         // 0 at the bottom, 2 at the top
		float drip = pow( VNoise( f.xz * 7.0 + 3.1 ), 3.0 ) * uDrip * 2.5; // some columns run much further
		float sag  = uMelt * h * ( 0.45 + drip ) * ( 0.6 + 0.4 * VNoise( f.xz * 2.3 ) );
		f.y -= sag;
		float floorY = -1.0 - uMelt * 0.15;
		float under  = max( floorY - f.y, 0.0 );
		f.y          = max( f.y, floorY - under * 0.08 );
		f.xz *= 1.0 + uMelt * 0.35 * smoothstep( 0.4, 0.0, f.y - floorY ); // puddle spreading on the floor
	}
#endif
	if( uVoxel > 0.0 )
		f = mix( f, ( floor( f / uVoxelSize ) + 0.5 ) * uVoxelSize, uVoxel );
	return f;
}

// ---- low poly: corners snap to the rotated grid; each facet (the grid cell holding the triangle's centroid)
// can shrink toward its centre (Facet Gap) and tumble about it
vec4 QAxis( vec3 a, float ang ) { return vec4( a * sin( ang * 0.5 ), cos( ang * 0.5 ) ); }
vec4 QMul( vec4 a, vec4 b ) { return vec4( a.w * b.xyz + b.w * a.xyz + cross( a.xyz, b.xyz ), a.w * b.w - dot( a.xyz, b.xyz ) ); }
vec3 QRot( vec4 q, vec3 v )
{
	vec3 t = 2.0 * cross( q.xyz, v );
	return v + q.w * t + cross( q.xyz, t );
}
vec4 QEuler( vec3 deg )
{
	vec3 r = radians( deg );
	return QMul( QAxis( vec3( 0, 0, 1 ), r.z ), QMul( QAxis( vec3( 0, 1, 0 ), r.y ), QAxis( vec3( 1, 0, 0 ), r.x ) ) );
}
vec4 QNlerp( vec4 a, vec4 b, float t ) { return normalize( mix( a, dot( a, b ) < 0.0 ? -b : b, t ) ); }
// steady turn + rocking about the rest pose (Sway 1 = rocking only); a facet's whole-number speed multiplies both
vec3 Sway( vec3 a, vec3 ph ) { return a + uTumbleSway * 35.0 * sin( radians( ph ) ); }

struct Facet
{
	vec3 c;  // centre (fitted space)
	vec4 q;  // rotation about it
};
Facet MakeFacet( vec3 mid )
{
	vec3 cell = floor( ( uLowPolyGrid * mid ) / uLowPolySize );
	Facet fc;
	fc.c = transpose( uLowPolyGrid ) * ( ( cell + 0.5 ) * uLowPolySize );
	fc.q = vec4( 0.0, 0.0, 0.0, 1.0 );
	if( uTumble == 0 && uFlipOn == 0 )
		return fc;
	float id = dot( cell, vec3( 1.0, 57.0, 113.0 ) );
	vec3 h   = Hash3v( id * 0.1337 + 3.7 ), h2 = Hash3v( id * 0.0731 + 11.3 );
	if( uTumble == 1 )
		fc.q = QEuler( Sway( uTumbleAng, uTumblePh ) );
	else if( uTumble == 2 )
	{
		// own axis frame and whole-number speed factors (-2, -1, 1, 2 per axis), so the angles wrap seamlessly;
		// Tumble Random blends from Together (0) to fully separate (1)
		vec3 v   = floor( h * 4.0 );
		vec3 sp  = v - 2.0 + step( 2.0, v );
		vec4 Q   = QAxis( normalize( h2 - 0.5 + 1e-4 ), h2.x * 6.2831853 );
		vec4 qS  = QMul( Q, QMul( QEuler( Sway( uTumbleAng * sp, uTumblePh * sp ) ), vec4( -Q.xyz, Q.w ) ) );
		fc.q     = QNlerp( QEuler( Sway( uTumbleAng, uTumblePh ) ), qS, uTumbleRandom );
	}
	if( uFlipOn == 1 )
	{
		bool sep    = uTumble == 2;
		float delay = sep ? h.z * 0.6 * uFlipDur * uTumbleRandom : 0.0;
		float u     = clamp( ( uFlipT - delay ) / max( uFlipDur, 1e-3 ), 0.0, 1.0 );
		u           = u * u * ( 3.0 - 2.0 * u );
		vec3 ax     = vec3( 1.0, 0.0, 0.0 );
		if( sep && h2.y < uTumbleRandom )
			ax = h2.z < 0.333 ? vec3( 1.0, 0.0, 0.0 ) : ( h2.z < 0.667 ? vec3( 0.0, 1.0, 0.0 ) : vec3( 0.0, 0.0, 1.0 ) );
		fc.q = QMul( QAxis( ax, radians( uFlipBase + uFlipDeg * u ) ), fc.q );
	}
	fc.q = QNlerp( vec4( 0.0, 0.0, 0.0, 1.0 ), fc.q, uTumbleW );
	return fc;
}
vec3 LowPoly( vec3 f, Facet fc )
{
	vec3 q = uLowPolyGrid * f;
	q      = floor( q / uLowPolySize + 0.5 ) * uLowPolySize;
	f      = mix( f, transpose( uLowPolyGrid ) * q, uLowPoly );
	return fc.c + QRot( fc.q, ( f - fc.c ) * ( 1.0 - 0.9 * uFacetGap ) );
}

void main()
{
	int tri    = gl_VertexID / 3;
	int corner = gl_VertexID - tri * 3;
	int base   = uFirstIndex + tri * 3;
	int i0 = int( texelFetch( uIdxBuf, base ).r ), i1 = int( texelFetch( uIdxBuf, base + 1 ).r ), i2 = int( texelFetch( uIdxBuf, base + 2 ).r );
#ifdef WIRE
	uint em = uWireEdges == 1 ? texelFetch( uEdgeMask, base / 3 ).r : 7u;
	vEdge   = vec3( float( em & 1u ), float( ( em >> 1u ) & 1u ), float( ( em >> 2u ) & 1u ) );
#endif
#ifdef DEFORM
	if( uFoldN > 1 )
	{
		// Mirror Fold: copy c keeps the side bit k of c says on its k-th folded axis
		int c = gl_InstanceID % uFoldN, k = 0;
		for( int a = 0; a < 3; ++a )
			if( uFoldAxes[ a ] > 0.5 )
				gFoldS[ a ] = ( ( c >> k++ ) & 1 ) == 1 ? -1.0 : 1.0;
		if( uFoldK > 0.5 )
		{
			// a triangle reflected an odd number of times turns its winding: swap two corners so it stays front-facing
			vec3 c3 = ( DeformPre( ( FetchPos( i0 ) - uFitCenter ) * uFitScale, FetchNrm( i0 ) ) + DeformPre( ( FetchPos( i1 ) - uFitCenter ) * uFitScale, FetchNrm( i1 ) ) +
			            DeformPre( ( FetchPos( i2 ) - uFitCenter ) * uFitScale, FetchNrm( i2 ) ) ) / 3.0;
			vec3 r     = step( 0.5, uFoldAxes ) * step( 0.0, -gFoldS * c3 );
			gFoldFlip  = mod( r.x + r.y + r.z, 2.0 ) > 0.5;
			if( gFoldFlip )
			{
				int sw = i1;
				i1 = i2, i2 = sw;
			}
		}
	}
#endif
	int me = corner == 0 ? i0 : ( corner == 1 ? i1 : i2 );

	int b   = me * 7;
	vec3 p  = FetchPos( me );
	vec2 uv = FetchUV( me );
	vec3 n  = FetchNrm( me );
	vec4 t  = UnpackSnorm( texelFetch( uVtxBuf, b + 6 ).r );

	if( uHas4 == 1 && uHeightAmt != 0.0 )
	{
		float h = dot( textureLod( uTex4, uv, 1.0 ), uSel4 );
		p += n * ( h - 0.5 ) * uHeightAmt / max( uModelScale, 1e-6 );
	}
	vec3 o0 = FetchPos( i0 ), o1 = FetchPos( i1 ), o2 = FetchPos( i2 );
	vec3 wn = normalize( uNormalMat * n );
	vec3 nObj = n;
	vec3 opos = ( p - uFitCenter ) * uFitScale;
#ifdef LETTERS
	vec3 lopos = opos; // patterns / dissolve stay stuck to the letter
	{
		int lid = int( texelFetch( uLetterOf, base / 3 ).r );
		vec4 lc = texelFetch( uLetterXf, lid * 3 ), lq = texelFetch( uLetterXf, lid * 3 + 1 ), lo = texelFetch( uLetterXf, lid * 3 + 2 );
#define LETTER( r ) ( ( lc.xyz + lo.xyz + QRot( lq, ( ( (r) - uFitCenter ) * uFitScale - lc.xyz ) * lc.w ) ) / uFitScale + uFitCenter )
		p  = LETTER( p );
		o0 = LETTER( o0 ), o1 = LETTER( o1 ), o2 = LETTER( o2 );
#undef LETTER
		n     = QRot( lq, n );
		t.xyz = QRot( lq, t.xyz );
		wn    = normalize( uNormalMat * n );
		nObj  = n;
		opos  = ( p - uFitCenter ) * uFitScale;
	}
#endif

	if( uShapeOn == 1 )
	{
		// every corner of the triangle takes the same path, so the triangle stays whole
		vec3 f  = Continuous( opos, n );
		vec3 f0 = Continuous( ( o0 - uFitCenter ) * uFitScale, FetchNrm( i0 ) );
		vec3 f1 = Continuous( ( o1 - uFitCenter ) * uFitScale, FetchNrm( i1 ) );
		vec3 f2 = Continuous( ( o2 - uFitCenter ) * uFitScale, FetchNrm( i2 ) );
#ifdef DEFORM
		{
			// the normal (and tangent) follow the deform: finite differences along the tangent / bitangent (a reflected copy's
			// differences turn inward: flipped back)
			vec3 Tn = t.xyz - n * dot( n, t.xyz );
			Tn      = dot( Tn, Tn ) > 1e-12 ? normalize( Tn ) : normalize( cross( n, abs( n.x ) < 0.9 ? vec3( 1, 0, 0 ) : vec3( 0, 1, 0 ) ) );
			vec3 Bn = cross( n, Tn );
			gNormalPass = true;
			vec3 f0n    = Continuous( opos, n );
			vec3 dT     = Continuous( opos + Tn * 0.01, n ) - f0n, dB = Continuous( opos + Bn * 0.01, n ) - f0n;
			gNormalPass = false;
			vec3 dn = cross( dT, dB );
			if( dot( dn, dn ) > 1e-16 )
				n = normalize( dn ) * ( gFoldFlip ? -1.0 : 1.0 );
			if( dot( dT, dT ) > 1e-16 )
				t.xyz = normalize( dT );
		}
#endif
		if( uLowPoly > 0.0 )
		{
			// the facet is the grid cell of the triangle's centroid: all three corners agree on it
			Facet fct = MakeFacet( ( f0 + f1 + f2 ) / 3.0 );
			f         = LowPoly( f, fct );
			f0 = LowPoly( f0, fct ), f1 = LowPoly( f1, fct ), f2 = LowPoly( f2, fct );
			n         = QRot( fct.q, n );
			t.xyz     = QRot( fct.q, t.xyz );
		}
		vec3 fc = ( f0 + f1 + f2 ) / 3.0;
		vec3 fn = cross( f1 - f0, f2 - f0 );
#ifdef DEFORM
		float flatK = max( uVoxel, uLowPoly ); // the continuous deforms carry their own normals (above)
#else
		float flatK = max( max( uVoxel, uLowPoly ), ( uMelt > 0.0 || uTwist != 0.0 || uBend != 0.0 || uRipple > 0.0 ) ? 1.0 : 0.0 );
#endif
		vec3 nOut = n;
		if( dot( fn, fn ) > 1e-24 && flatK > 0.0 )
			nOut = normalize( mix( n, normalize( fn ), clamp( flatK, 0.0, 1.0 ) ) );
		if( uLowPoly > 0.5 )
			uv = ( FetchUV( i0 ) + FetchUV( i1 ) + FetchUV( i2 ) ) / 3.0; // flat-coloured facets
		// slices: whole triangles move with the slab their centroid falls in
		if( uSliceGap > 0.0 || uSliceWave > 0.0 || uSlide != 0.0 )
		{
			vec3 qc   = ToAxis( fc, uSliceAxis );
			float idx = clamp( floor( ( qc.z + 1.0 ) * 0.5 * uSliceCount ), 0.0, uSliceCount - 1.0 );
			vec3 off  = vec3( 0.0, 0.0, ( idx - 0.5 * ( uSliceCount - 1.0 ) ) * uSliceGap * 2.0 / uSliceCount );
			f += FromAxis( off, uSliceAxis );
			// wave and slide run across the screen: along X, or along Y for X slices
			vec3 side = uSliceAxis == 0 ? vec3( 0.0, 1.0, 0.0 ) : vec3( 1.0, 0.0, 0.0 );
			f += side * ( sin( idx * 0.9 + uSlicePhase ) * uSliceWave * 0.25 + uSlide * 3.2 * ( 0.6 + 0.8 * fract( idx * 0.618 ) ) );
		}
		// shatter: whole shards fly, spin and fall, and retrace the path when they come back
		if( uShatterT > 0.0 && uShardsReady == 1 )
		{
			uint s   = texelFetch( uShardOf, base / 3 ).r;
			vec4 sd  = texelFetch( uShardData, int( s ) );
			vec3 h   = Hash3v( sd.w * 17.31 );
			vec3 dir = normalize( sd.xyz + ( h - 0.5 ) * 0.9 + vec3( 0.0, 0.0, 0.35 ) );
			vec3 vel = dir * uShatterForce * ( 0.5 + h.x );
			vec3 ax  = normalize( Hash3v( sd.w * 5.13 + 2.0 ) - 0.5 + 1e-4 );
#if defined( BOUNCE )
			// W26 Shatter Physics Bounce: the shard's recorded flight (60 Hz, 4 s: offset + rotation), read at the flight time,
			// so Reform (the time running back) retraces it
			float fr = clamp( uShatterT * 60.0, 0.0, 240.0 );
			int i0   = int( floor( fr ) ), i1 = min( i0 + 1, 240 );
			float a  = fr - float( i0 );
			int pb   = int( s ) * 482;
			vec3 off = mix( texelFetch( uShardPath, pb + i0 * 2 ).xyz, texelFetch( uShardPath, pb + i1 * 2 ).xyz, a );
			vec4 q   = QNlerp( texelFetch( uShardPath, pb + i0 * 2 + 1 ), texelFetch( uShardPath, pb + i1 * 2 + 1 ), a );
			mat3 R   = mat3( QRot( q, vec3( 1.0, 0.0, 0.0 ) ), QRot( q, vec3( 0.0, 1.0, 0.0 ) ), QRot( q, vec3( 0.0, 0.0, 1.0 ) ) );
			f        = sd.xyz + off + R * ( f - sd.xyz );
#elif defined( TRANS )
			// transition: the rim leaves first and the centre last (jittered); each shard flies over its own
			// stretch of the slider, eased at both ends, and shrinks away over the far half of its flight
			float k  = clamp( 0.9 - 0.6 * length( sd.xy ) + 0.5 * ( h.z - 0.5 ), 0.0, 1.0 );
			float u  = clamp( ( uTransE - uTransStagger * k ) / ( 1.0 - uTransStagger ), 0.0, 1.0 );
			float a  = uTransSide == 1 ? u : 1.0 - u;
			float T  = uShatterT * mix( a, a * a * ( 3.0 - 2.0 * a ), uTransEase );
			mat3 R   = RotAxis( ax, uShardSpin * T * ( 0.4 + h.y ) );
			f        = sd.xyz + R * ( f - sd.xyz ) * ( 1.0 - uTransEase * smoothstep( 0.4, 1.0, a ) ) + vel * T + vec3( 0.0, -0.5 * uShatterGravity * T * T, 0.0 );
#else
			mat3 R   = RotAxis( ax, uShardSpin * uShatterT * ( 0.4 + h.y ) );
			float T  = uShatterT;
			f        = sd.xyz + R * ( f - sd.xyz ) + vel * T + vec3( 0.0, -0.5 * uShatterGravity * T * T, 0.0 );
#endif
			nOut     = R * nOut;
			t.xyz    = R * t.xyz;
		}
#ifdef CRACKS
		// W26 lava split: every triangle moves with the crack cell of its centroid, out from the mask's centre, once the
		// cracks have reached that cell's site
		{
			vec4 cc  = CrackCell( fc * uCrackScale );
			vec3 sit = cc.yzw / uCrackScale;
			float on = 1.0 - smoothstep( uCrackFront - 0.15, uCrackFront, length( sit - uCrackSeed ) );
			vec3 mv  = ( normalize( sit + vec3( 0.0, 0.0, 0.25 ) ) * 0.12 + ( CrackHash( floor( cc.yzw * 7.0 ) ) - 0.5 ) * 0.05 ) * uSplit * on;
			f += mv, f0 += mv, f1 += mv, f2 += mv;
		}
#endif
		p    = f / uFitScale + uFitCenter;
		wn   = normalize( uNormalMat * nOut );
		nObj = nOut;
		o0 = f0 / uFitScale + uFitCenter, o1 = f1 / uFitScale + uFitCenter, o2 = f2 / uFitScale + uFitCenter;
	}

#ifdef INSTANCED
	// clone / ghost: its own model matrix (rigid + uniform scale, so mat3(M) is a normal matrix up to length)
	mat4 M  = uInst[ uInstBase + INST_ID ];
	wn      = normalize( mat3( M ) * nObj );
	vInstHA = uInstHA[ uInstBase + INST_ID ];
	vInstX  = uInstX[ uInstBase + INST_ID ];
#else
#define M uModel
	vInstHA = vec2( 0.0, 1.0 );
#endif
	vec3 w  = ( M * vec4( p, 1.0 ) ).xyz;
	vec3 w0 = ( M * vec4( o0, 1.0 ) ).xyz;
	vec3 w1 = ( M * vec4( o1, 1.0 ) ).xyz;
	vec3 w2 = ( M * vec4( o2, 1.0 ) ).xyz;
	vec3 fn   = cross( w1 - w0, w2 - w0 );
	float len = length( fn );
	fn        = len > 1e-20 ? fn / len : wn;
	vec3 c    = ( w0 + w1 + w2 ) / 3.0;

	if( uExplode > 0.0 )
	{
		float id  = float( base / 3 );
		float r1  = Hash1( id );
		vec3 rdir = normalize( vec3( Hash1( id + 1.7 ), Hash1( id + 3.1 ), Hash1( id + 5.3 ) ) * 2.0 - 1.0 + 1e-4 );
		vec3 dir  = normalize( mix( fn, rdir, 0.6 * uExplodeRandom ) );
		float mag = uExplode * mix( 1.0, r1 * 2.0, uExplodeRandom ) * 0.35;
		float shrink = clamp( 1.0 - 0.15 * uExplode, 0.3, 1.0 );
		w = c + ( w - c ) * shrink + dir * mag * uWorldScale;
	}
#ifdef INSTANCED
	if( uTrailSmear > 0.0 )
		w += TrailSmear( w, p, wn );
	w += TrailDrift( vInstX.z );
#endif
	if( uGlitchAmt > 0.0 )
		w = Glitch( w );
	vo.wpos     = w;
	vo.wnrm     = wn;
	vo.wtan     = vec4( normalize( mat3( M ) * t.xyz ), t.w < 0.0 ? -1.0 : 1.0 );
	vo.uv       = uv;
#ifdef LETTERS
	vo.opos     = lopos;
#else
	vo.opos     = opos;
#endif
	vo.bary     = corner == 0 ? vec3( 1, 0, 0 ) : ( corner == 1 ? vec3( 0, 1, 0 ) : vec3( 0, 0, 1 ) );
	gl_Position = uViewProj * vec4( w, 1.0 );
}
)GLSL";


// Extra lights shared by the surface shader, the fog march and the motes: up to 6 spot / point lights
// (0-1 = the volume lights A/B, which may carry a shadow map; 2-5 = the sweeping spots).
static const char* kLightLib = R"GLSL(
const int MAX_XL = 8;
uniform int uXLCount;
uniform vec3 uXLPos[ MAX_XL ];
uniform vec3 uXLDir[ MAX_XL ];
uniform vec3 uXLColor[ MAX_XL ];  // colour x intensity
uniform vec2 uXLCone[ MAX_XL ];   // cos(outer half angle), cos(inner half angle); x <= -1.0 = point light
uniform float uXLRange;           // shaft length: distance window
uniform mat4 uXLightVP[ 4 ];      // volume lights 0..3: A, B, C left, C right (as enabled)
uniform int uXShadowMode[ 4 ];    // 0 none, 1 spot (2D), 2 point (cube: lights 0 / 1 only)
uniform sampler2D uXShadow2D0;
uniform sampler2D uXShadow2D1;
uniform sampler2D uXShadow2D2;
uniform sampler2D uXShadow2D3;
uniform samplerCube uXShadowCube0;
uniform samplerCube uXShadowCube1;

float XLWindow( float d )
{
	float x = d / max( uXLRange, 1e-3 );
	float w = clamp( 1.0 - x * x * x * x, 0.0, 1.0 );
	return w * w / ( 1.0 + d * d );
}
float XLCone( int i, vec3 l ) // l: unit vector light -> point
{
	vec2 c = uXLCone[ i ];
	return c.x <= -1.0 ? 1.0 : smoothstep( c.x, c.y, dot( l, uXLDir[ i ] ) );
}
// 1 = lit. The shadow maps store the distance to the light.
float XShadow( int i, vec3 p, float d )
{
	if( i > 3 )
		return 1.0;
	int m = uXShadowMode[ i ];
	if( m == 0 )
		return 1.0;
	float bias = 0.02 + 0.01 * d;
	if( m == 2 )
	{
		vec3 v  = p - uXLPos[ i ];
		float s = i == 0 ? textureLod( uXShadowCube0, v, 0.0 ).r : textureLod( uXShadowCube1, v, 0.0 ).r;
		return step( d - bias, s );
	}
	vec4 c = uXLightVP[ i ] * vec4( p, 1.0 );
	if( c.w <= 1e-5 )
		return 1.0;
	vec2 uv = c.xy / c.w * 0.5 + 0.5;
	if( any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) )
		return 1.0;
	float s = i == 0 ? textureLod( uXShadow2D0, uv, 0.0 ).r
	        : i == 1 ? textureLod( uXShadow2D1, uv, 0.0 ).r
	        : i == 2 ? textureLod( uXShadow2D2, uv, 0.0 ).r
	                 : textureLod( uXShadow2D3, uv, 0.0 ).r;
	return step( d - bias, s );
}
)GLSL";

static const char* kMainFS = R"GLSL(
in VData
{
	vec3 wpos;
	vec3 wnrm;
	vec4 wtan;
	vec2 uv;
	vec3 opos;
	vec3 bary;
} vi;

out vec4 fragColor;
flat in vec2 vInstHA; // clone hue offset, ghost alpha
#ifdef WIRE
// W5: the WIRE twins. uWirePass 0 depth only, 1 dark fill (+ lines), 2 lines only, 3 over the surface (the W1 formula), 4 the lines'
// glow into the HDR wire buffer (light at alpha 0)
flat in vec3 vEdge;
uniform int uWirePass;
uniform vec3 uWireColor;
uniform float uWireColorMix;
uniform float uWireFade, uWireFadeNear, uWireFadeFar;
#endif
#ifdef INSTANCED
flat in vec4 vInstX;   // keep, fog keep, trail age
uniform int uTrailPass; // 1 = the ghosts (trail copies)
uniform float uTrailDust;
uniform float uTrailGrain; // grain cell, fitted object units
uniform vec3 uTrailColor;
uniform float uTrailMix;
uniform float uTrailGlow;
#endif

// per part
uniform sampler2D uTex0; // base color (sRGB)
uniform sampler2D uTex1; // normal
uniform sampler2D uTex2; // roughness
uniform sampler2D uTex3; // metallic
uniform sampler2D uTex4; // height (VS)
uniform sampler2D uTex5; // emissive (sRGB)
uniform sampler2D uTex6; // AO
uniform int uHas[ 7 ];
uniform vec4 uSel[ 7 ];
uniform vec4 uBaseFactor;
uniform vec3 uEmisFactor;
uniform float uRoughFactor;
uniform float uMetalFactor;
uniform float uNormalScale;
uniform float uFlipGreen; // -1 or 1
uniform int uCutout;
uniform float uPartHue;
uniform float uPartGlow;
uniform int uPartSkin;
uniform int uPartId;

// global material
uniform vec3 uTint;
uniform float uTintMix;
uniform float uHue;
uniform float uSaturation;
uniform float uBrightness;
uniform int uMetalOverride;
uniform float uMetalValue;
uniform float uRoughScale;
uniform float uNormalStrength;
uniform float uEmissiveGlow;
uniform float uRimGlow;
uniform vec3 uRimColor;

// lighting
uniform vec3 uCamPos;
uniform vec3 uKeyDir;
uniform vec3 uKeyColor;
uniform vec3 uFillDir;
uniform vec3 uFillColor;
uniform float uAmbient;
uniform float uEnvIntensity;
uniform int uEnvSource; // 0 studio, 1 input, 2 off
uniform float uEnvRotate;
uniform float uExposure;
uniform int uTonemap; // 0 ACES, 1 filmic, 2 none
uniform mat4 uView;

// input (FX)
uniform sampler2D uInputTex; // mipmapped copy, sRGB-encoded values
uniform float uInputMaxLod;
uniform int uInputUse;        // 0 off, 1 background, 2 skin flagged, 3 skin all
uniform int uInputProjection; // 0 front planar, 1 model uv, 2 spherical
uniform float uInputMix;

// fx
uniform float uWireMix;
uniform float uWireThickness;
uniform float uDissolve;
uniform float uDissolveEdge;
uniform float uToonSteps;
uniform float uFogDensity;
uniform float uFogStart;
uniform vec4 uBackground; // premultiplied, sRGB-encoded
uniform int uDebugView;
uniform int uOutlinePass;

// ---- Stage 3 surface effects (all scaled by the part's Surface amount)
uniform int uSurfaceOn;
uniform float uPartSurface;
uniform float uTime;
uniform float uBeat;
uniform float uAudioMid;
uniform float uFilm, uFilmThick, uFilmShift, uFilmSwirl;
uniform float uGlitter, uGlitterDensity, uGlitterSize, uGlitterMetal;
uniform float uGlitterPhase; // twinkles (integrated Glitter Twinkle), wrapped at 1000
uniform float uChrome, uChromeRipple;
uniform vec2 uChromeOff;     // ripple drift (y, z), wrapped at 64: Noise3P tiles every 64
uniform vec3 uFilmT;         // swirl drift, the same drift one loop back, cross-fade weight toward it
uniform float uGlass, uGlassIOR, uGlassDisp;
uniform vec3 uGlassTint;
uniform int uGlassInput; // FX build with an input: refract the incoming layer
uniform vec2 uViewport;
uniform float uHolo, uHoloScan, uHoloFlicker, uHoloSplit;
uniform float uHoloPhase;    // scanline crawl, radians (wrapped)
uniform vec3 uHoloColor;
uniform float uHeat, uHeatTemp, uHeatSpread;
// ---- Stage 4: contour lines + blow to dust (the mesh dissolves where the dust has left)
uniform float uContour;
uniform int uContourMode; // 0 X, 1 Y, 2 Z, 3 height map, 4 radial
uniform float uContourSpacing;
uniform float uContourWidth;
uniform float uContourScroll;
uniform vec3 uContourColor;
uniform float uDustCut; // > 0: fragments whose dust noise is below this have blown away
uniform int uDustInvert; // transition: the incoming model keeps exactly those fragments
uniform int uDissolveInvert;
uniform int uPattern; // 0 off, 1 stripes, 2 cells, 3 noise
uniform float uPatAmt, uPatScale, uPatBeat, uPatAudio;
uniform vec3 uPatT;          // crawl (integrated Pattern Speed), the same one loop back, cross-fade weight toward it
uniform vec3 uPatColor;
#ifdef SHADOW_PASS
uniform vec3 uShadowLightPos;
uniform vec3 uShadowDir;   // ortho (directional) shadow: depth along the light's travel
uniform int uShadowOrtho;
#endif
#ifdef PHOTON_PASS
uniform int uPhotonOut;
#endif
#ifdef CRACK_PASS
uniform float uLavaGlow, uLavaTemp, uCrackWidth, uLavaTintMix, uCrackFlow;
uniform vec3 uLavaTint;
#endif
#ifdef PROJ_PASS
// W26: the cookie-light pass (its own program variants, drawn additively over the mask): up to 2 lights whose texture holds
// the projected picture (rgb) and the shadow distance (a) in the light's own projection (Projector; Stage 8 caustics)
uniform int uPLCount;
uniform mat4 uPLVP[ 2 ];
uniform vec3 uPLPos[ 2 ];
uniform vec3 uPLColor[ 2 ];
uniform sampler2D uPLTex0;
uniform sampler2D uPLTex1;
uniform int uPLExact[ 2 ]; // 1 = light only AT the stored distance (caustics land on their receiver), 0 = a shadow test
#endif

const float PI = 3.14159265;

float Sel( int i, vec4 s ) { return dot( s, uSel[ i ] ); }

vec3 HueRotate( vec3 c, float turns )
{
	float a       = turns * 6.2831853;
	const vec3 k  = vec3( 0.57735 );
	float ca      = cos( a );
	return c * ca + cross( k, c ) * sin( a ) + k * dot( k, c ) * ( 1.0 - ca );
}
float Luma( vec3 c ) { return dot( c, vec3( 0.2126, 0.7152, 0.0722 ) ); }

// ---- noise for dissolve
float Hash3( vec3 p )
{
	p = fract( p * 0.3183099 + 0.1 );
	p *= 17.0;
	return fract( p.x * p.y * p.z * ( p.x + p.y + p.z ) );
}
float Noise3( vec3 x )
{
	vec3 i = floor( x );
	vec3 f = fract( x );
	f      = f * f * ( 3.0 - 2.0 * f );
	return mix( mix( mix( Hash3( i + vec3( 0, 0, 0 ) ), Hash3( i + vec3( 1, 0, 0 ) ), f.x ),
	                 mix( Hash3( i + vec3( 0, 1, 0 ) ), Hash3( i + vec3( 1, 1, 0 ) ), f.x ), f.y ),
	            mix( mix( Hash3( i + vec3( 0, 0, 1 ) ), Hash3( i + vec3( 1, 0, 1 ) ), f.x ),
	                 mix( Hash3( i + vec3( 0, 1, 1 ) ), Hash3( i + vec3( 1, 1, 1 ) ), f.x ), f.y ),
	            f.z );
}
float Fbm( vec3 p )
{
	float v = 0.0, a = 0.5;
	for( int i = 0; i < 4; ++i )
	{
		v += a * Noise3( p );
		p *= 2.03;
		a *= 0.5;
	}
	return v / 0.9375;
}
// Noise3 on a lattice that repeats every 64 cells on each axis: a drift wrapped at 64 is seamless
float Noise3P( vec3 x )
{
	vec3 i = floor( x );
	vec3 f = fract( x );
	f      = f * f * ( 3.0 - 2.0 * f );
	vec3 a = mod( i, 64.0 ), b = mod( i + 1.0, 64.0 );
	return mix( mix( mix( Hash3( vec3( a.x, a.y, a.z ) ), Hash3( vec3( b.x, a.y, a.z ) ), f.x ),
	                 mix( Hash3( vec3( a.x, b.y, a.z ) ), Hash3( vec3( b.x, b.y, a.z ) ), f.x ), f.y ),
	            mix( mix( Hash3( vec3( a.x, a.y, b.z ) ), Hash3( vec3( b.x, a.y, b.z ) ), f.x ),
	                 mix( Hash3( vec3( a.x, b.y, b.z ) ), Hash3( vec3( b.x, b.y, b.z ) ), f.x ), f.y ),
	            f.z );
}
// Cross-fade of two independent fields (mean 0.5) that keeps their contrast: w 0 = a, 1 = b
float LoopBlend( float a, float b, float w )
{
	return 0.5 + ( ( 1.0 - w ) * ( a - 0.5 ) + w * ( b - 0.5 ) ) / sqrt( ( 1.0 - w ) * ( 1.0 - w ) + w * w );
}

// ---- procedural studio environment
float Softbox( vec3 d, vec3 c, vec2 hs, float r )
{
	float facing = dot( d, c );
	if( facing <= 0.0 )
		return 0.0;
	vec3 t     = normalize( abs( c.y ) > 0.99 ? cross( c, vec3( 1, 0, 0 ) ) : cross( c, vec3( 0, 1, 0 ) ) );
	vec3 b     = cross( t, c );
	vec2 p     = vec2( dot( d, t ), dot( d, b ) ) / facing;
	float blur = 0.015 + r * r * 1.6;
	float s    = ( 1.0 - smoothstep( hs.x - blur, hs.x + blur, abs( p.x ) ) ) * ( 1.0 - smoothstep( hs.y - blur, hs.y + blur, abs( p.y ) ) );
	return s * ( hs.x * hs.y ) / ( ( hs.x + blur ) * ( hs.y + blur ) );
}
vec3 Studio( vec3 d, float r )
{
	float ca = cos( uEnvRotate ), sa = sin( uEnvRotate );
	d        = vec3( ca * d.x + sa * d.z, d.y, -sa * d.x + ca * d.z );
	vec3 col = mix( vec3( 0.015, 0.015, 0.018 ), vec3( 0.16, 0.17, 0.19 ), smoothstep( -0.25, 0.7, d.y ) );
	col += vec3( 0.06 ) * ( 1.0 - smoothstep( 0.0, 0.12 + r * 0.3, abs( d.y + 0.02 ) ) );
	col += Softbox( d, normalize( vec3( -0.65, 0.55, 0.55 ) ), vec2( 0.45, 0.3 ), r ) * vec3( 7.0, 6.8, 6.4 );
	col += Softbox( d, normalize( vec3( 0.85, 0.15, 0.25 ) ), vec2( 0.07, 0.5 ), r ) * vec3( 4.5, 4.7, 5.2 );
	col += Softbox( d, normalize( vec3( 0.1, 1.0, -0.1 ) ), vec2( 0.35, 0.35 ), r ) * vec3( 2.2 );
	col += Softbox( d, normalize( vec3( 0.2, 0.1, -1.0 ) ), vec2( 0.6, 0.12 ), r ) * vec3( 1.6, 1.4, 1.2 );
	return col;
}
vec3 SrgbToLinear( vec3 c ) { return pow( max( c, 0.0 ), vec3( 2.2 ) ); }
vec2 SphereMapUV( vec3 wdir )
{
	vec3 v  = mat3( uView ) * wdir;
	float m = 2.0 * sqrt( v.x * v.x + v.y * v.y + ( v.z + 1.0 ) * ( v.z + 1.0 ) );
	return v.xy / max( m, 1e-4 ) + 0.5; // host textures are bottom-up: +y view = +v
}
vec3 Env( vec3 d, float r )
{
	if( uEnvSource == 2 )
		return vec3( 0.0 );
	if( uEnvSource == 1 )
		return SrgbToLinear( textureLod( uInputTex, SphereMapUV( d ), r * uInputMaxLod ).rgb ) * 2.0;
	return Studio( d, r );
}
vec2 EnvBRDF( float r, float nov )
{
	const vec4 c0 = vec4( -1.0, -0.0275, -0.572, 0.022 );
	const vec4 c1 = vec4( 1.0, 0.0425, 1.04, -0.04 );
	vec4 rr       = r * c0 + c1;
	float a004    = min( rr.x * rr.x, exp2( -9.28 * nov ) ) * rr.x + rr.y;
	return vec2( -1.04, 1.04 ) * a004 + rr.zw;
}
float DGGX( float noh, float a )
{
	float a2 = a * a;
	float d  = noh * noh * ( a2 - 1.0 ) + 1.0;
	return a2 / ( PI * d * d );
}
float VisSmith( float nov, float nol, float a )
{
	float k = a * 0.5;
	return 0.25 / ( ( nov * ( 1.0 - k ) + k ) * ( nol * ( 1.0 - k ) + k ) );
}
vec3 Tonemap( vec3 c )
{
	if( uTonemap == 0 )
	{
		c *= 0.6;
		return clamp( ( c * ( 2.51 * c + 0.03 ) ) / ( c * ( 2.43 * c + 0.59 ) + 0.14 ), 0.0, 1.0 );
	}
	if( uTonemap == 1 )
	{
		const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30, W = 11.2;
		vec3 x        = c * 2.0;
		vec3 m        = ( ( x * ( A * x + C * B ) + D * E ) / ( x * ( A * x + B ) + D * F ) ) - E / F;
		float w       = ( ( W * ( A * W + C * B ) + D * E ) / ( W * ( A * W + B ) + D * F ) ) - E / F;
		return clamp( m / w, 0.0, 1.0 );
	}
	return clamp( c, 0.0, 1.0 );
}
vec3 Encode( vec3 c ) { return pow( max( c, 0.0 ), vec3( 1.0 / 2.2 ) ); }

// Kelvin -> linear rgb, max component 1 (Helland's fit)
vec3 Blackbody( float k )
{
	float t = clamp( k, 800.0, 15000.0 ) / 100.0;
	vec3 c;
	c.r = t <= 66.0 ? 1.0 : clamp( 1.292936 * pow( t - 60.0, -0.1332047592 ), 0.0, 1.0 );
	c.g = t <= 66.0 ? clamp( 0.3900815788 * log( t ) - 0.6318414438, 0.0, 1.0 ) : clamp( 1.129890861 * pow( t - 60.0, -0.0755148492 ), 0.0, 1.0 );
	c.b = t >= 66.0 ? 1.0 : ( t <= 19.0 ? 0.0 : clamp( 0.5432067891 * log( t - 10.0 ) - 1.19625408914, 0.0, 1.0 ) );
	return pow( c, vec3( 2.2 ) );
}
// thin-film interference of a soap / oil layer (n = 1.33), thickness in nm
vec3 ThinFilm( float cosV, float nm )
{
	float n    = 1.33;
	float cosT = sqrt( max( 1.0 - ( 1.0 - cosV * cosV ) / ( n * n ), 0.0 ) );
	float opd  = 2.0 * n * nm * cosT;
	return 0.5 + 0.5 * cos( 6.2831853 * opd / vec3( 650.0, 532.0, 450.0 ) );
}
vec2 Hash22( vec2 p )
{
	p = vec2( dot( p, vec2( 127.1, 311.7 ) ), dot( p, vec2( 269.5, 183.3 ) ) );
	return fract( sin( p ) * 43758.5453 );
}
// crawling patterns on one planar projection
float PatternAt( vec2 p, float t )
{
	if( uPattern == 1 )
	{
		float s = sin( ( p.x + p.y * 0.35 - t ) * 6.2831853 );
		return smoothstep( 0.2, 0.9, s );
	}
	if( uPattern == 2 )
	{
		p += vec2( t * 0.6, t * 0.25 );
		vec2 ip = floor( p ), fp = fract( p );
		float d1 = 8.0, d2 = 8.0;
		for( int y = -1; y <= 1; ++y )
			for( int x = -1; x <= 1; ++x )
			{
				vec2 o  = vec2( x, y );
				vec2 c  = o + 0.5 + 0.4 * sin( t + 6.2831853 * Hash22( ip + o ) ) - fp;
				float d = dot( c, c );
				if( d < d1 )
					d2 = d1, d1 = d;
				else if( d < d2 )
					d2 = d;
			}
		return 1.0 - smoothstep( 0.0, 0.12, sqrt( d2 ) - sqrt( d1 ) );
	}
	return smoothstep( 0.45, 0.75, Fbm( vec3( p, t * 0.5 ) ) );
}
// the pattern at the crawl phase; near the loop's end, cross-faded into the pattern one loop back (where it continues)
float PatternX( vec2 p )
{
	if( uPatT.z <= 0.0 )
		return PatternAt( p, uPatT.x );
	if( uPattern == 3 ) // noise: blend the fields before the threshold, so the blobs keep their contrast
		return smoothstep( 0.45, 0.75, LoopBlend( Fbm( vec3( p, uPatT.x * 0.5 ) ), Fbm( vec3( p, uPatT.y * 0.5 ) ), uPatT.z ) );
	return mix( PatternAt( p, uPatT.x ), PatternAt( p, uPatT.y ), uPatT.z );
}

// Host input textures are bottom-up (GL), unlike model textures (top row first), so every
// projection here returns v with +v = up.
vec2 InputUV()
{
	if( uInputProjection == 1 )
		return vec2( fract( vi.uv.x ), 1.0 - fract( vi.uv.y ) );
	if( uInputProjection == 2 )
	{
		vec3 d = normalize( vi.opos + 1e-6 );
		return vec2( atan( d.x, d.z ) / 6.2831853 + 0.5, 0.5 + asin( clamp( d.y, -1.0, 1.0 ) ) / PI );
	}
	return vec2( vi.opos.x * 0.5 + 0.5, vi.opos.y * 0.5 + 0.5 );
}

void main()
{
	// derivatives first: after a discard they are undefined on Apple's GL-over-Metal
	vec3 bw = max( fwidth( vi.bary ), vec3( 1e-6 ) );
	float cc = 0.0, cfw = 1.0;
	if( uContour > 0.0 )
	{
		cc  = uContourMode == 0 ? vi.opos.x : uContourMode == 1 ? vi.opos.y : uContourMode == 2 ? vi.opos.z
		    : uContourMode == 3 ? ( uHas[ 4 ] == 1 ? dot( texture( uTex4, vi.uv ), uSel[ 4 ] ) : Luma( texture( uTex0, vi.uv ).rgb ) ) * 2.0
		    : length( vi.opos );
		cfw = max( fwidth( cc ), 1e-6 );
	}
	if( uOutlinePass == 1 )
	{
		fragColor = vec4( 0.0, 0.0, 0.0, 1.0 );
		return;
	}

	if( uDustCut > 0.0 && ( ( Fbm( vi.opos * 3.0 + 17.0 ) < uDustCut * 1.08 - 0.04 ) != ( uDustInvert == 1 ) ) )
		discard;
	// dissolve
	float edgeGlow = 0.0;
	if( uDissolve > 0.0 )
	{
		float n  = Fbm( vi.opos * 3.0 + 7.0 );
		float th = uDissolve * 1.08 - 0.04;
		if( ( n < th ) != ( uDissolveInvert == 1 ) )
			discard;
		edgeGlow = uDissolveInvert == 1 ? smoothstep( th - 0.03, th, n ) : 1.0 - smoothstep( th, th + 0.03, n );
	}
#ifdef INSTANCED
	// W15: a tunnel ring fading in / out (the W1 dissolve noise; the shadow maps fade with it, fog keep included), and
	// a ghost turning to dust with age: a grain hash on its own surface (the grains travel with the ghost's drift)
	float keep = vInstX.x;
#ifdef SHADOW_PASS
	keep *= vInstX.y;
#endif
	if( keep < 1.0 && Fbm( vi.opos * 3.0 + 29.0 ) < ( 1.0 - keep ) * 1.08 - 0.04 )
		discard;
	if( uTrailPass == 1 && uTrailDust > 0.0 && Hash3( mod( floor( vi.opos / max( uTrailGrain, 1e-4 ) ), 256.0 ) ) < uTrailDust * smoothstep( 0.05, 0.8, vInstX.z ) )
		discard;
#endif

	vec2 uv   = vi.uv;
	vec4 base = uBaseFactor;
	if( uHas[ 0 ] == 1 )
		base *= texture( uTex0, uv );
	if( uCutout == 1 && base.a < 0.5 )
		discard;
#ifdef SHADOW_PASS
	fragColor = vec4( uShadowOrtho == 1 ? dot( vi.wpos - uShadowLightPos, uShadowDir ) : length( vi.wpos - uShadowLightPos ) );
	return;
#else

	if( uInputUse == 3 || ( uInputUse == 2 && uPartSkin == 1 ) )
	{
		vec3 skin = SrgbToLinear( texture( uInputTex, InputUV() ).rgb );
		base.rgb  = mix( base.rgb, skin, uInputMix );
	}

	vec3 albedo = HueRotate( base.rgb, uHue + uPartHue + vInstHA.x );
	albedo      = max( mix( vec3( Luma( albedo ) ), albedo, uSaturation ), 0.0 );
	albedo      = mix( albedo, uTint * Luma( albedo ) * 2.0, uTintMix );
	albedo *= uBrightness;

	float rough = uRoughFactor;
	if( uHas[ 2 ] == 1 )
		rough *= Sel( 2, texture( uTex2, uv ) );
	rough       = clamp( rough * uRoughScale, 0.03, 1.0 );
	float metal = uMetalFactor;
	if( uHas[ 3 ] == 1 )
		metal *= Sel( 3, texture( uTex3, uv ) );
	metal    = uMetalOverride == 1 ? uMetalValue : clamp( metal, 0.0, 1.0 );
	float ao = uHas[ 6 ] == 1 ? Sel( 6, texture( uTex6, uv ) ) : 1.0;

	vec3 Ng = normalize( vi.wnrm );
	vec3 T  = normalize( vi.wtan.xyz - Ng * dot( Ng, vi.wtan.xyz ) );
	float w = vi.wtan.w;
	if( !gl_FrontFacing )
	{
		Ng = -Ng;
		T  = -T;
	}
	vec3 B = cross( Ng, T ) * w * ( gl_FrontFacing ? 1.0 : -1.0 );
	vec3 N = Ng;
	if( uHas[ 1 ] == 1 )
	{
		vec3 nt = texture( uTex1, uv ).rgb * 2.0 - 1.0;
		nt.y *= uFlipGreen;
		nt.xy *= uNormalScale * uNormalStrength;
		N = normalize( T * nt.x + B * nt.y + Ng * max( nt.z, 1e-3 ) );
	}

#ifdef PHOTON_PASS
	// W26 caustics: the refractor seen from the caustic light: world position (0) or shading normal (1), a = coverage
	fragColor = uPhotonOut == 0 ? vec4( vi.wpos, 1.0 ) : vec4( N, 1.0 );
	return;
#endif
	vec3 emis = uEmisFactor;
	if( uHas[ 5 ] == 1 )
		emis *= texture( uTex5, uv ).rgb;

	if( uDebugView > 0 )
	{
		vec3 d;
		if( uDebugView == 1 )
			d = Encode( albedo );
		else if( uDebugView == 2 )
			d = N * 0.5 + 0.5;
		else if( uDebugView == 3 )
			d = vec3( rough );
		else if( uDebugView == 4 )
			d = vec3( metal );
		else if( uDebugView == 5 )
			d = vec3( fract( uv ), 0.0 );
		else
		{
			const vec3 pal[ 8 ] = vec3[ 8 ]( vec3( 1, .25, .2 ), vec3( .2, .8, .3 ), vec3( .25, .45, 1 ), vec3( 1, .85, .2 ),
			                                 vec3( .85, .3, 1 ), vec3( .2, .9, .95 ), vec3( 1, .6, .2 ), vec3( .9, .9, .9 ) );
			d = pal[ uPartId % 8 ] * ( 0.55 + 0.45 * max( dot( N, normalize( uCamPos - vi.wpos ) ), 0.0 ) );
		}
		fragColor = vec4( d, 1.0 );
		return;
	}

	vec3 V    = normalize( uCamPos - vi.wpos );
	float nov = max( dot( N, V ), 1e-4 );
	float sAmt = uSurfaceOn == 1 ? uPartSurface : 0.0;
	vec3 filmTint = vec3( 1.0 );
	if( sAmt > 0.0 )
	{
		if( uChrome > 0.0 )
		{
			float k = uChrome * sAmt;
			if( uChromeRipple > 0.0 )
			{
				vec3 q   = vi.opos * 5.0 + vec3( 0.0, uChromeOff );
				float n0 = Noise3P( q );
				vec3 g   = vec3( Noise3P( q + vec3( 0.05, 0, 0 ) ), Noise3P( q + vec3( 0, 0.05, 0 ) ), Noise3P( q + vec3( 0, 0, 0.05 ) ) ) - n0;
				N        = normalize( N - ( g - N * dot( g, N ) ) * 20.0 * uChromeRipple * k );
				nov      = max( dot( N, V ), 1e-4 );
			}
			albedo = mix( albedo, vec3( 0.96, 0.95, 0.93 ), k );
			metal  = mix( metal, 1.0, k );
			rough  = mix( rough, 0.035, k );
		}
		if( uFilm > 0.0 )
		{
			float swirl = 0.0;
			if( uFilmSwirl > 0.0 )
			{
				swirl = Fbm( vi.opos * 2.5 + vec3( uFilmT.x ) );
				if( uFilmT.z > 0.0 )
					swirl = LoopBlend( swirl, Fbm( vi.opos * 2.5 + vec3( uFilmT.y ) ), uFilmT.z );
				swirl *= uFilmSwirl;
			}
			float nm    = uFilmThick * ( 250.0 + 700.0 * swirl );
			float cv    = mix( 1.0, nov, clamp( uFilmShift, 0.0, 2.0 ) * 0.5 );
			filmTint    = mix( vec3( 1.0 ), ThinFilm( cv, nm ) * 1.6, clamp( uFilm * sAmt, 0.0, 1.0 ) );
			albedo *= mix( vec3( 1.0 ), filmTint, 0.5 );
		}
	}
	vec3 F0   = mix( vec3( 0.04 ), albedo, metal ) * filmTint;
	vec3 diff = albedo * ( 1.0 - metal );
	float a   = rough * rough;

	vec3 color = vec3( 0.0 );
#if defined( CRACK_PASS )
	float surfAlpha = 1.0;
	{
		// W26 lava cracks: the Voronoi borders glow (blackbody), behind a spreading front that burns hottest
		vec4 cc    = CrackCell( vi.opos * uCrackScale );
		float e    = cc.x / uCrackScale;           // fitted units to the border
		float dS   = length( vi.opos - uCrackSeed );
		float on   = 1.0 - smoothstep( uCrackFront - 0.06, uCrackFront, dS );
		float fresh = exp( -max( uCrackFront - dS, 0.0 ) * 6.0 ); // the newest cracks are hottest
		float w    = uCrackWidth * 0.03 * ( 0.5 + 0.5 * on ) + 1e-4;
		float aa   = max( fwidth( e ), 1e-5 );
		float core = 1.0 - smoothstep( w * 0.5 - aa, w * 0.5 + aa, e );
		float halo = exp( -e / ( w * 1.5 ) ) * 0.35;
		float flow = Noise3P( vi.opos * 6.0 + vec3( 0.0, uCrackFlow, 0.0 ) );
		float T    = uLavaTemp * ( 0.55 + 0.45 * core ) * ( 0.9 + 0.2 * flow ) * ( 1.0 + 0.35 * fresh ); // W28: calmer flow (it read as breathing)
		vec3 bb    = Blackbody( T );
		bb         = mix( bb, uLavaTint * dot( bb, vec3( 0.3333 ) ) * 1.5, uLavaTintMix );
		color      = bb * pow( T / uLavaTemp, 4.0 ) * uLavaGlow * ( core + halo ) * on * 2.0;
		// the crust darkens round the cracks (light pass: only the glow is added here)
	}
#elif defined( PROJ_PASS )
	float surfAlpha = 1.0;
	for( int i = 0; i < uPLCount; ++i )
	{
		vec4 c = uPLVP[ i ] * vec4( vi.wpos, 1.0 );
		if( c.w <= 1e-5 )
			continue;
		vec2 puv = c.xy / c.w * 0.5 + 0.5;
		vec2 eg  = min( puv, 1.0 - puv );
		if( min( eg.x, eg.y ) <= 0.0 )
			continue;
		vec4 t    = i == 0 ? textureLod( uPLTex0, puv, 0.0 ) : textureLod( uPLTex1, puv, 0.0 );
		vec3 Lv   = uPLPos[ i ] - vi.wpos;
		float d   = length( Lv );
		vec3 L    = Lv / max( d, 1e-5 );
		float nol = max( dot( N, L ), 0.0 );
		float lit = ( uPLExact[ i ] == 1 ? 1.0 - smoothstep( 0.03 + 0.01 * d, 0.06 + 0.02 * d, abs( d - t.a ) ) : step( d - ( 0.02 + 0.01 * d ), t.a ) ) * smoothstep( 0.0, 0.004, min( eg.x, eg.y ) );
		if( nol * lit <= 0.0 )
			continue;
		vec3 H     = normalize( L + V );
		float noh  = max( dot( N, H ), 0.0 );
		float voh  = max( dot( V, H ), 0.0 );
		vec3 F     = F0 + ( 1.0 - F0 ) * pow( 1.0 - voh, 5.0 );
		float spec = DGGX( noh, a ) * VisSmith( nov, max( nol, 1e-4 ), a );
		color += ( diff / PI + F * spec ) * uPLColor[ i ] * t.rgb * ( nol * lit / ( 1.0 + d * d ) );
	}
#else
	for( int li = 0; li < 2; ++li )
	{
		vec3 L    = li == 0 ? uKeyDir : uFillDir;
		vec3 lc   = li == 0 ? uKeyColor : uFillColor;
		float nol = max( dot( N, L ), 0.0 );
		if( uToonSteps > 0.5 )
			nol = floor( nol * uToonSteps + 0.5 ) / uToonSteps;
		vec3 H    = normalize( L + V );
		float noh = max( dot( N, H ), 0.0 );
		float voh = max( dot( V, H ), 0.0 );
		vec3 F    = F0 + ( 1.0 - F0 ) * pow( 1.0 - voh, 5.0 );
		float spec = DGGX( noh, a ) * VisSmith( nov, max( nol, 1e-4 ), a );
		if( uToonSteps > 0.5 )
			spec = step( 1.0, spec * nol ) * 2.0;
		color += ( diff / PI + F * spec ) * lc * nol;
	}
	for( int i = 0; i < uXLCount; ++i )
	{
		vec3 Lv   = uXLPos[ i ] - vi.wpos;
		float d   = length( Lv );
		vec3 L    = Lv / max( d, 1e-5 );
		float nol = max( dot( N, L ), 0.0 );
		float k   = XLCone( i, -L ) * XLWindow( d ) * nol;
		if( k <= 0.0 )
			continue;
		k *= XShadow( i, vi.wpos + Ng * 0.01, d );
		vec3 H     = normalize( L + V );
		float noh  = max( dot( N, H ), 0.0 );
		float voh  = max( dot( V, H ), 0.0 );
		vec3 F     = F0 + ( 1.0 - F0 ) * pow( 1.0 - voh, 5.0 );
		float spec = DGGX( noh, a ) * VisSmith( nov, max( nol, 1e-4 ), a );
		color += ( diff / PI + F * spec ) * uXLColor[ i ] * k;
	}
	vec3 R       = reflect( -V, N );
	vec2 ab      = EnvBRDF( rough, nov );
	vec3 envDiff = Env( N, 1.0 );
	if( uToonSteps > 0.5 )
		envDiff = floor( envDiff * uToonSteps + 0.5 ) / uToonSteps;
	vec3 envSpec = Env( R, rough ) * ( F0 * ab.x + ab.y );
	color += ( diff * envDiff + envSpec ) * uEnvIntensity * ao;
	color += albedo * uAmbient * ao;

	float fres = pow( 1.0 - nov, 3.0 );
	color += uRimColor * uRimGlow * fres;
	color += emis * uEmissiveGlow;
	color += albedo * uPartGlow;
	color += uRimColor * edgeGlow * uDissolveEdge * 2.0;

	if( uContour > 0.0 )
	{
		float sp   = max( uContourSpacing, 1e-3 );
		float x    = fract( cc / sp - uContourScroll );
		float d    = min( x, 1.0 - x ) * sp;
		float hw   = uContourWidth * sp * 0.5;
		float line = 1.0 - smoothstep( hw - cfw, hw + cfw, d );
		float halo = exp( -d / ( sp * 0.15 ) ) * 0.35;
		color += uContourColor * ( line + halo ) * uContour;
	}
	float surfAlpha = 1.0;
	if( sAmt > 0.0 )
	{
		if( uGlitter > 0.0 )
		{
			vec3 cp = vi.opos * ( 36.0 / max( uGlitterSize, 0.05 ) );
			vec3 ci = floor( cp );
			float h = Hash3( ci );
			if( h < uGlitterDensity )
			{
				vec3 jn     = normalize( N + ( vec3( Hash3( ci + 1.31 ), Hash3( ci + 2.73 ), Hash3( ci + 4.19 ) ) - 0.5 ) * 1.4 );
				float flake = smoothstep( 0.5, 0.15, length( fract( cp ) - 0.5 ) );
				// Twinkle = twinkles per second per flake; each flake has its own phase and a +-20 % rate, rounded to a
				// whole number of twinkles per 1000 so the phase's wrap at 1000 is seamless
				float rate  = floor( ( 0.8 + 0.4 * Hash3( ci + 7.7 ) ) * 1000.0 + 0.5 ) / 1000.0;
				float tw    = 0.5 + 0.5 * sin( 6.2831853 * fract( uGlitterPhase * rate ) + h * 97.0 );
				vec3 rj     = reflect( -V, jn );
				float kspec = pow( max( dot( jn, normalize( V + uKeyDir ) ), 0.0 ), 28.0 );
				float espec = pow( max( dot( rj, normalize( vec3( -0.65, 0.55, 0.55 ) ) ), 0.0 ), 16.0 )
				            + pow( max( dot( rj, normalize( vec3( 0.85, 0.15, 0.25 ) ) ), 0.0 ), 16.0 )
				            + pow( max( dot( rj, vec3( 0.0, 0.0, 1.0 ) ), 0.0 ), 16.0 );
				float w     = mix( 1.0, metal, clamp( uGlitterMetal, 0.0, 1.0 ) );
				vec3 tint   = mix( vec3( 1.0 ), albedo / max( max( albedo.r, max( albedo.g, albedo.b ) ), 1e-3 ), metal * 0.7 );
				color += ( uKeyColor * kspec + vec3( 3.0 ) * espec ) * tint * flake * tw * tw * uGlitter * w * sAmt * 2.5;
			}
		}
		if( uHeat > 0.0 )
		{
			float hh = uHas[ 4 ] == 1 ? Sel( 4, texture( uTex4, uv ) ) : Luma( base.rgb );
			float T  = uHeatTemp * mix( 1.0, 0.3 + 1.4 * hh, clamp( uHeatSpread, 0.0, 1.0 ) );
			color += Blackbody( T ) * pow( T / 1600.0, 4.0 ) * 0.12 * uHeat * sAmt;
		}
		if( uPattern > 0 && uPatAmt > 0.0 )
		{
			vec3 q  = vi.opos * uPatScale;
			vec3 bw3 = pow( abs( Ng ), vec3( 4.0 ) );
			bw3 /= bw3.x + bw3.y + bw3.z + 1e-5;
			float pv = PatternX( q.yz ) * bw3.x + PatternX( q.xz ) * bw3.y + PatternX( q.xy ) * bw3.z;
			color += uPatColor * pv * uPatAmt * ( 1.0 + uPatBeat * uBeat * 2.0 + uPatAudio * uAudioMid ) * sAmt;
		}
		if( uGlass > 0.0 )
		{
			float k   = clamp( uGlass * sAmt, 0.0, 1.0 );
			float ior = max( uGlassIOR, 1.0001 );
			vec3 disp = vec3( -1.0, 0.0, 1.0 ) * uGlassDisp * 0.06;
			vec3 refr;
			if( uGlassInput == 1 )
			{
				vec2 suv = gl_FragCoord.xy / uViewport;
				for( int c = 0; c < 3; ++c )
				{
					vec3 rd = refract( -V, N, 1.0 / ( ior + disp[ c ] ) );
					vec2 o  = ( mat3( uView ) * rd ).xy * 0.12 * ( ior - 1.0 ) * 2.0;
					refr[ c ] = SrgbToLinear( textureLod( uInputTex, suv + o, 1.0 ).rgb )[ c ];
				}
			}
			else
				for( int c = 0; c < 3; ++c )
					refr[ c ] = Env( refract( -V, N, 1.0 / ( ior + disp[ c ] ) ), 0.05 )[ c ];
			float fr   = 0.04 + 0.96 * pow( 1.0 - nov, 5.0 );
			vec3 glass = refr * uGlassTint * ( 1.0 - fr ) + Env( reflect( -V, N ), 0.02 ) * fr * uEnvIntensity;
			color      = mix( color, glass, k );
			if( uGlassInput == 0 )
				surfAlpha = mix( surfAlpha, clamp( 0.25 + fr + Luma( glass ) * 0.5, 0.0, 1.0 ), k );
		}
		if( uHolo > 0.0 )
		{
			float k   = clamp( uHolo * sAmt, 0.0, 1.0 );
			float fr  = pow( 1.0 - nov, 1.5 );
			float sp  = uHoloSplit * 0.004;
			vec3 scan = 0.55 + 0.45 * sin( ( vi.wpos.y + vec3( sp, 0.0, -sp ) ) * 140.0 * max( uHoloScan, 0.05 ) - uHoloPhase );
			float fl  = 1.0 - uHoloFlicker * 0.45 * step( 0.72, fract( sin( floor( uTime * 24.0 ) * 12.9898 ) * 43758.5453 ) );
			vec3 hc   = uHoloColor * scan * ( 0.2 + 2.2 * fr ) * fl * 2.0;
			color     = mix( color, hc, k );
			surfAlpha = mix( surfAlpha, clamp( ( 0.12 + 0.9 * fr ) * ( 0.6 + 0.4 * scan.g ) * fl, 0.0, 1.0 ), k );
		}
	}

#endif
#ifdef INSTANCED
	if( uTrailPass == 1 ) // ghosts take the trail colour (keeping their shading) and glow in it
		color = mix( color, uTrailColor * ( 0.15 + 1.2 * Luma( color ) ), uTrailMix ) + uTrailColor * uTrailGlow * 0.25;
#endif
	color *= exp2( uExposure );
	vec3 outc   = Encode( Tonemap( color ) );
	// Branch-free on purpose: a discard inside this block miscompiles on Apple's GL-over-Metal.
#ifdef WIRE
	// an edge the flags leave out (Feature Edges) never draws: its barycentric distance counts as far
	vec3 e      = max( smoothstep( vec3( 0.0 ), bw * uWireThickness, vi.bary ), vec3( 1.0 ) - vEdge );
	float wire  = 1.0 - min( min( e.x, e.y ), e.z );
	wire       *= 1.0 - uWireFade * clamp( ( length( uCamPos - vi.wpos ) - uWireFadeNear ) / max( uWireFadeFar - uWireFadeNear, 1e-4 ), 0.0, 1.0 );
	vec3 lineC  = mix( min( outc * 1.6 + 0.08, 1.0 ), uWireColor, uWireColorMix );
	float wa    = wire * uWireMix;
	float alpha = 1.0;
	if( uWirePass == 3 )
	{
		outc  = mix( outc, lineC, wa );
		alpha = mix( 1.0, wire, uWireMix ) * surfAlpha * vInstHA.y;
	}
	else if( uWirePass == 1 )
		outc = lineC * wa, alpha = vInstHA.y;
	else if( uWirePass == 2 || uWirePass == 4 )
		outc = lineC, alpha = wa * vInstHA.y;
#else
	vec3 e      = smoothstep( vec3( 0.0 ), bw * uWireThickness, vi.bary );
	float wire  = 1.0 - min( min( e.x, e.y ), e.z );
	outc        = mix( outc, min( outc * 1.6 + 0.08, 1.0 ), wire * uWireMix );
	float alpha = mix( 1.0, wire, uWireMix ) * surfAlpha * vInstHA.y;
#endif

	vec4 result = vec4( outc * alpha, alpha );
	if( uFogDensity > 0.0 )
	{
		float dist = length( uCamPos - vi.wpos );
		float fog  = 1.0 - exp( -uFogDensity * max( dist - uFogStart, 0.0 ) );
#ifdef INSTANCED
		fog = max( fog, 1.0 - vInstX.y ); // the tunnel's far end: rings arrive out of the fog
#endif
		result     = mix( result, uBackground, fog );
	}
	fragColor = result;
#if defined( PROJ_PASS ) || defined( CRACK_PASS )
	fragColor = vec4( result.rgb, 0.0 ); // added light: the mask's coverage is already there
#endif
#ifdef WIRE
	if( uWirePass == 0 )
		fragColor = vec4( 0.0 ); // depth only (alpha stays 1 for the discard test: the depth is written)
	else if( uWirePass == 4 )
		fragColor = vec4( lineC * alpha, 0.0 ); // light, added into the wire buffer
#endif
	if( alpha < 0.004 )
		discard;
#endif
}
)GLSL";

static const char* kQuadVS = R"GLSL(
out vec2 vUV;
void main()
{
	vec2 p      = vec2( ( gl_VertexID << 1 ) & 2, gl_VertexID & 2 );
	vUV         = p;
	gl_Position = vec4( p * 2.0 - 1.0, 0.0, 1.0 );
}
)GLSL";

// Composites the resolved model (premultiplied) over the background into the host FBO, then the volume
// light (fog) on top: Add = premultiplied additive glow (alpha untouched), Over = alpha from the density.
// The fog may be rendered at half / quarter res: it is upsampled here, weighted by depth similarity.
static const char* kCompositeFS = R"GLSL(
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uScene;
uniform sampler2D uInput;
uniform vec2 uInputScale;
uniform int uUseInput;
uniform vec4 uBackground; // premultiplied
uniform int uFogOn;
uniform int uFogBlend;     // 0 add, 1 over
uniform int uFogDepthAware;
uniform sampler2D uFog;
uniform sampler2D uFogDepth;
uniform sampler2D uDepth;
uniform vec2 uFogSize;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
// floor / wall under the scene (Stage 2): shadow catcher + mirror
uniform int uFloorOn;
uniform vec4 uPlane;         // mirror floor: n.xyz, d with dot( n, p ) + d = 0
uniform vec4 uCatchPlane;    // shadow catcher: floor or wall
uniform float uShadowAmt;
uniform float uShadowSoft;
uniform float uFloorLight;      // x Shadow Catcher: a soft key-lit pool round the mask, so the shadow reads over black
uniform vec3 uFloorLightColor;
uniform mat4 uKeyVP;
uniform vec3 uKeyPos;
uniform vec3 uKeyDir;        // direction the key light travels
uniform sampler2D uKeyShadow;
uniform int uMirrorOn;
uniform sampler2D uMirror;
uniform float uMirrorAmt;
uniform float uMirrorBlur;
uniform float uMirrorFade;
uniform vec3 uMaskCenter;
// silhouette (Stage 2): black fill, glowing outline
uniform int uSilOn;
uniform float uSil;
uniform float uSilFill;
uniform float uSilGlow;
uniform vec3 uSilColor;
uniform sampler2D uSilBlur;  // blurred scene alpha
uniform vec2 uTexel;
// echo trails (Stage 5): under the live mask
uniform int uEchoOn;
uniform sampler2D uEcho;

vec4 FogSample()
{
	if( uFogDepthAware == 0 )
		return texture( uFog, vUV );
	float z  = texture( uDepth, vUV ).r;
	float tf = 1e4;
	if( z < 1.0 )
	{
		vec4 s = uInvViewProj * vec4( vUV * 2.0 - 1.0, z * 2.0 - 1.0, 1.0 );
		tf     = min( length( s.xyz / s.w - uCamPos ), 1e4 );
	}
	vec2 fc  = vUV * uFogSize - 0.5;
	vec2 i0  = floor( fc );
	vec2 fr  = fc - i0;
	vec4 acc = vec4( 0.0 );
	float ws = 0.0;
	for( int k = 0; k < 4; ++k )
	{
		vec2 o   = vec2( k & 1, k >> 1 );
		vec2 uv  = ( i0 + o + 0.5 ) / uFogSize;
		float bw = ( o.x > 0.5 ? fr.x : 1.0 - fr.x ) * ( o.y > 0.5 ? fr.y : 1.0 - fr.y );
		float tl = textureLod( uFogDepth, uv, 0.0 ).r;
		float w  = bw / ( 1e-3 + abs( tl - tf ) / max( tf, 1e-3 ) * 20.0 );
		acc += textureLod( uFog, uv, 0.0 ) * w;
		ws += w;
	}
	return ws > 1e-6 ? acc / ws : texture( uFog, vUV );
}

const vec2 kPoisson[ 12 ] = vec2[ 12 ]( vec2( -0.326, -0.406 ), vec2( -0.840, -0.074 ), vec2( -0.696, 0.457 ), vec2( -0.203, 0.621 ),
                                         vec2( 0.962, -0.195 ), vec2( 0.473, -0.480 ), vec2( 0.519, 0.767 ), vec2( 0.185, -0.893 ),
                                         vec2( 0.507, 0.064 ), vec2( 0.896, 0.412 ), vec2( -0.322, -0.933 ), vec2( -0.792, -0.598 ) );

// The shadow catcher and the mirror live on invisible planes under the scene.
bool PlaneHit( vec4 pl, out vec3 P )
{
	vec4 f     = uInvViewProj * vec4( vUV * 2.0 - 1.0, 1.0, 1.0 );
	vec3 rd    = normalize( f.xyz / f.w - uCamPos );
	float side = dot( pl.xyz, uCamPos ) + pl.w;
	float den  = dot( pl.xyz, rd );
	P          = uCamPos + rd * ( -side / min( den, -1e-5 ) );
	return side > 0.0 && den < -1e-5;
}
vec4 FloorLayer( vec4 bg )
{
	vec3 P;
	if( uMirrorOn == 1 && PlaneHit( uPlane, P ) )
	{
		float dist = length( ( P - uMaskCenter ) - uPlane.xyz * dot( P - uMaskCenter, uPlane.xyz ) );
		float fade = exp( -dist * ( 0.15 + 3.0 * uMirrorFade ) );
		float r    = uMirrorBlur * ( 0.004 + 0.03 * dist );
		vec4 m     = texture( uMirror, vUV );
		for( int i = 0; i < 12; ++i )
			m += texture( uMirror, vUV + kPoisson[ i ] * r * vec2( uTexel.y / uTexel.x, 1.0 ) );
		m /= 13.0;
		bg = m * ( uMirrorAmt * fade ) + bg * ( 1.0 - m.a * uMirrorAmt * fade );
	}
	if( uShadowAmt > 0.0 && PlaneHit( uCatchPlane, P ) )
	{
		vec4 c   = uKeyVP * vec4( P, 1.0 );
		vec2 uv  = c.xy / c.w * 0.5 + 0.5;
		float oc = 0.0;
		if( all( greaterThan( uv, vec2( 0.0 ) ) ) && all( lessThan( uv, vec2( 1.0 ) ) ) )
		{
			float d = dot( P - uKeyPos, uKeyDir );
			float r = 0.002 + uShadowSoft * 0.03;
			for( int i = 0; i < 12; ++i )
				oc += step( texture( uKeyShadow, uv + kPoisson[ i ] * r ).r, d - 0.03 );
			oc /= 12.0;
		}
		if( uFloorLight > 0.0 )
		{
			// the pool is lit by the key, so the shadow cuts it
			float dist = length( ( P - uMaskCenter ) - uCatchPlane.xyz * dot( P - uMaskCenter, uCatchPlane.xyz ) );
			float a    = uFloorLight * exp( -dist * dist * 0.35 ) * ( 1.0 - oc );
			bg         = vec4( uFloorLightColor * a, a ) + bg * ( 1.0 - a );
		}
		bg = bg * ( 1.0 - oc * uShadowAmt ) + vec4( 0.0, 0.0, 0.0, oc * uShadowAmt );
	}
	return bg;
}

#ifdef HAZE
// W26 caustics on the catcher plane (the same variant as the haze; uCausticOn gates it): the caustic map (light space, a =
// the receiver distance) projected onto the plane where the light reaches it, added (light at alpha 0)
uniform int uCausticOn;
uniform sampler2D uCaustic;
uniform mat4 uCausticVP;
uniform vec3 uCausticPos, uCausticColor;
vec3 CausticFloor()
{
	vec3 P;
	if( uCausticOn == 0 || !PlaneHit( uCatchPlane, P ) )
		return vec3( 0.0 );
	vec4 c = uCausticVP * vec4( P, 1.0 );
	if( c.w <= 1e-5 )
		return vec3( 0.0 );
	vec2 uv = c.xy / c.w * 0.5 + 0.5;
	vec2 e  = min( uv, 1.0 - uv );
	if( min( e.x, e.y ) <= 0.0 )
		return vec3( 0.0 );
	vec4 t  = texture( uCaustic, uv );
	float d = length( P - uCausticPos );
	return uCausticColor * t.rgb * step( d - 0.05 - 0.01 * d, t.a ) * smoothstep( 0.0, 0.02, min( e.x, e.y ) );
}
// W26 heat haze (its own composite variant): the picture shimmers where the lava cracks are (a half-res mask of their glow,
// read a little below each pixel too: the heat rises). The phase wraps at 1; it is read at x3 and x2 (whole cycles).
uniform sampler2D uHazeMask;
uniform float uHazeAmt, uHazePhase;
vec2 HazeOffset()
{
	float m = 0.0;
	for( int k = 0; k < 8; ++k )
	{
		vec2 o = vec2( float( k % 3 ) - 1.0, -float( k / 3 ) ) * vec2( 0.012, 0.018 );
		m += dot( texture( uHazeMask, vUV + o ).rgb, vec3( 0.3333 ) );
	}
	m = min( m * 0.25, 1.5 );
	float a = sin( 6.2831853 * ( vUV.y * 22.0 - 3.0 * uHazePhase ) + 2.0 * sin( vUV.x * 37.0 ) );
	float b = sin( 6.2831853 * ( vUV.x * 17.0 + 2.0 * uHazePhase ) + 2.0 * sin( vUV.y * 29.0 ) );
	return vec2( a, b ) * m * uHazeAmt * 0.004;
}
#endif
void main()
{
#ifdef HAZE
	vec2 hz = uHazeAmt > 0.0 ? HazeOffset() : vec2( 0.0 );
	vec4 s  = texture( uScene, vUV + hz );
	vec4 bg = uUseInput == 1 ? texture( uInput, ( vUV + hz ) * uInputScale ) : uBackground;
	bg.rgb += CausticFloor();
#else
	vec4 s  = texture( uScene, vUV );
	vec4 bg = uUseInput == 1 ? texture( uInput, vUV * uInputScale ) : uBackground;
#endif
	if( uFloorOn == 1 )
		bg = FloorLayer( bg );
	if( uEchoOn == 1 )
	{
		vec4 e = texture( uEcho, vUV );
		bg     = e + bg * ( 1.0 - e.a );
	}
	if( uSilOn == 1 )
	{
		// black fill (the Fill slider reveals the lit mask) + a glow outside the silhouette and a crisp edge
		float a   = s.a;
		float bl  = texture( uSilBlur, vUV ).a;
		float mn  = min( min( texture( uScene, vUV + vec2( uTexel.x, 0.0 ) ).a, texture( uScene, vUV - vec2( uTexel.x, 0.0 ) ).a ),
		                 min( texture( uScene, vUV + vec2( 0.0, uTexel.y ) ).a, texture( uScene, vUV - vec2( 0.0, uTexel.y ) ).a ) );
		float edge = clamp( a - mn, 0.0, 1.0 );
		float glow = clamp( bl * ( 1.0 - a ) * 2.0, 0.0, 1.0 );
		vec4 fill  = vec4( s.rgb * uSilFill, a );
		s          = mix( s, fill, uSil );
		vec3 g     = uSilColor * ( glow + edge ) * uSilGlow * uSil;
		s.rgb += g;
		s.a = max( s.a, clamp( max( g.r, max( g.g, g.b ) ), 0.0, 1.0 ) );
	}
	fragColor = s + bg * ( 1.0 - s.a );
	if( uFogOn == 1 )
	{
		vec4 f  = FogSample();
		vec3 fl = ToDisplay( f.rgb );
		if( uFogBlend == 1 )
		{
			// Over: the lit medium covers what is behind it; unlit haze stays clear
			float a   = clamp( max( fl.r, max( fl.g, fl.b ) ), 0.0, 1.0 );
			fragColor = vec4( fl, a ) + fragColor * ( 1.0 - a );
		}
		else
			fragColor.rgb += fl;
	}
}
)GLSL";

// Copies the host input (possibly padded to hardware size) into a mipmappable texture.
static const char* kCopyFS = R"GLSL(
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uInput;
uniform vec2 uInputScale;
void main() { fragColor = texture( uInput, vUV * uInputScale ); }
)GLSL";
}
