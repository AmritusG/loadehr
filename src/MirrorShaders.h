#pragma once
// W26 Stage 10: the infinity mirror room. Bounces are UNFOLDED (as InfiniCube): a ray reflected in the box's mirrors is a
// straight ray through a lattice of mirrored copies of the box; a cell's bounce count = |i| + |j| + |k|. The mask's copies are
// drawn instanced (MIRROR programs: tint^bounces); this pass draws the LED strips on the box's edges through the same lattice:
// the distance to an edge line = the length of the two smallest per-axis distances to the cell walls (only the edges the
// shape has), sphere-traced from where the ray enters the box, glowing, dimmed per bounce, hidden behind the scene's depth.

namespace mehr
{
// W27: the lattice for everything that is not the mesh. A cell = xyz its index (odd = mirrored on that axis), w its bounces.
static const char* kMirrorLib = R"GLSL(
uniform vec3 uMBoxC, uMBoxH; // the real box (world): centre, half size
uniform vec3 uMEye;          // the camera
vec3 MirrorS( vec4 cell ) { return 1.0 - 2.0 * mod( cell.xyz, 2.0 ); }
vec3 MirrorUnfold( vec3 x, vec4 cell ) { return MirrorS( cell ) * ( x - uMBoxC ) + uMBoxC + 2.0 * uMBoxH * cell.xyz; }
// the camera's ray along rd passes through the box: the reflections are only seen through it (as the W26 copies)
bool MirrorThrough( vec3 rd )
{
	vec3 inv = 1.0 / ( rd + vec3( 1e-7 ) );
	vec3 t0  = ( uMBoxC - uMBoxH - uMEye ) * inv, t1 = ( uMBoxC + uMBoxH - uMEye ) * inv;
	vec3 tn  = min( t0, t1 ), tx = max( t0, t1 );
	return min( min( tx.x, tx.y ), tx.z ) > max( max( tn.x, tn.y ), max( tn.z, 0.0 ) );
}
bool MirrorInBox( vec3 p ) { return all( lessThanEqual( abs( p - uMBoxC ), uMBoxH ) ); }
)GLSL";

// particles / bolts: the program draws once per cell; MirrorClip puts a real point into the cell (off screen unless the camera
// sees it through the box)
static const char* kMirrorPointLib = R"GLSL(
uniform vec4 uMCell;
uniform vec3 uMTint; // the cell's tint^bounces
vec4 MirrorClip( mat4 vp, vec3 p )
{
	vec3 u = MirrorUnfold( p, uMCell );
	return MirrorInBox( p ) && MirrorThrough( u - uMEye ) ? vp * vec4( u, 1.0 ) : vec4( 0.0, 0.0, 0.0, -1.0 ); // w < 0: clipped
}
)GLSL";

// volumes: the camera ray runs straight on through the unfolded lattice; each sample is folded back into the real box (its
// density, heat and light are read there, along the folded ray direction) and dimmed by tint^bounces of its cell
static const char* kMirrorMarchLib = R"GLSL(
uniform vec3 uMAxes;  // 1 = a mirrored axis
uniform vec3 uMTint;  // per bounce
uniform int uMBounces;
uniform int uMSteps;  // steps through the reflections
// x = where the ray leaves the real box through a mirror, y = where its reflections end (Mirror Count bounces, an open side
// of the room, the scene); x >= y: no reflections on this ray
vec2 MirrorRange( vec3 rd, float tEnd )
{
	vec3 inv = 1.0 / ( rd + vec3( 1e-7 ) );
	vec3 t0  = ( uMBoxC - uMBoxH - uMEye ) * inv, t1 = ( uMBoxC + uMBoxH - uMEye ) * inv;
	vec3 tn  = min( t0, t1 ), tx = max( t0, t1 );
	float ta = max( max( tn.x, tn.y ), max( tn.z, 0.0 ) ), tb = min( min( tx.x, tx.y ), tx.z );
	if( tb <= ta || tb >= tEnd )
		return vec2( 1.0, 0.0 );
	// the front is a two-way mirror: a ray running out through it (+z) leaves the room (the copies are cells k <= 0 too)
	vec3 axes = uMAxes * vec3( 1.0, 1.0, rd.z < 0.0 ? 1.0 : 0.0 );
	vec3 dtc  = 2.0 * uMBoxH * abs( inv ); // between two walls, per axis
	vec3 tc   = tx;                        // the next wall crossed, per axis (the first: the box exit)
	int b     = 0;
	for( int i = 0; i < 12; ++i )
	{
		float t = min( tc.x, min( tc.y, tc.z ) );
		int a   = tc.x <= t ? 0 : ( tc.y <= t ? 1 : 2 );
		if( t >= tEnd )
			return vec2( tb, tEnd );
		if( axes[ a ] < 0.5 )
			return i == 0 ? vec2( 1.0, 0.0 ) : vec2( tb, t );
		if( ++b > uMBounces )
			return vec2( tb, t );
		tc[ a ] += dtc[ a ];
	}
	return vec2( tb, min( tEnd, min( tc.x, min( tc.y, tc.z ) ) ) );
}
// the cells the ray runs through after it leaves the box (nearest first, at most 9: Mirror Count 8 + an open side), w = bounces
int MirrorCells( vec3 rd, float tEnd, out vec4 cells[ 9 ] )
{
	vec2 mr = MirrorRange( rd, tEnd );
	if( mr.x >= mr.y )
		return 0;
	vec3 inv = 1.0 / ( rd + vec3( 1e-7 ) );
	vec3 t0  = ( uMBoxC - uMBoxH - uMEye ) * inv, t1 = ( uMBoxC + uMBoxH - uMEye ) * inv;
	vec3 tc  = max( t0, t1 ), dtc = 2.0 * uMBoxH * abs( inv );
	int n    = 0;
	for( int i = 0; i < 9; ++i )
	{
		float ta = min( tc.x, min( tc.y, tc.z ) );
		int a    = tc.x <= ta ? 0 : ( tc.y <= ta ? 1 : 2 );
		if( ta >= mr.y )
			break;
		tc[ a ] += dtc[ a ];
		float tb = min( mr.y, min( tc.x, min( tc.y, tc.z ) ) );
		vec3 c   = floor( ( uMEye + rd * ( 0.5 * ( ta + tb ) ) - uMBoxC + uMBoxH ) / ( 2.0 * uMBoxH ) ) * uMAxes;
		cells[ n++ ] = vec4( c, abs( c.x ) + abs( c.y ) + abs( c.z ) );
	}
	return n;
}
// an unfolded point -> the real one (and the ray direction there); b = the bounces of its cell
vec3 MirrorFold( vec3 u, vec3 rd, out vec3 rdF, out float b )
{
	vec3 c = floor( ( u - uMBoxC + uMBoxH ) / ( 2.0 * uMBoxH ) ) * uMAxes; // open axes stay in cell 0
	vec3 S = 1.0 - 2.0 * mod( c, 2.0 );
	rdF    = S * rd;
	b      = abs( c.x ) + abs( c.y ) + abs( c.z );
	return S * ( u - uMBoxC - 2.0 * uMBoxH * c ) + uMBoxC;
}
)GLSL";

static const char* kMirrorLedFS = R"GLSL(
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uDepth;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform vec3 uBoxC, uBoxH;   // centre, half size (world)
uniform int uShape;          // 1 box, 2 corridor (the four upright walls), 3 two mirrors (front + back)
uniform int uBounces;
uniform vec3 uTint;          // per bounce (x reflectance)
uniform vec3 uLed;           // colour x glow
uniform int uRainbow;
uniform float uWidth;        // strip radius, world
uniform float uPixAng;
uniform float uPhase;        // the chase, cycles (wraps at 1)
vec3 Hue( float h ) { return clamp( abs( fract( h + vec3( 0.0, 2.0 / 3.0, 1.0 / 3.0 ) ) * 6.0 - 3.0 ) - 1.0, 0.0, 1.0 ); }
// x = distance to the nearest strip, y = the coordinate along it (0..1 in its cell), z = the strip's axis
vec3 Strip( vec3 q, vec3 sz )
{
	vec3 f = fract( q ), a = min( f, 1.0 - f ) * sz;
	float ex = length( a.yz ), ey = length( a.xz ), ez = length( a.xy ); // the edges along x, y, z
	if( uShape == 3 )
		ez = 1e9;      // two mirrors: the rims of the front / back mirrors
	if( ex <= ey && ex <= ez )
		return vec3( ex, f.x, 0.0 );
	if( ey <= ez )
		return vec3( ey, f.y, 1.0 );
	return vec3( ez, f.z, 2.0 );
}
void main()
{
	vec4 f     = uInvViewProj * vec4( vUV * 2.0 - 1.0, 1.0, 1.0 );
	vec3 rd    = normalize( f.xyz / f.w - uCamPos );
	float z    = texture( uDepth, vUV ).r;
	float tEnd = 1e6;
	if( z < 1.0 )
	{
		vec4 s = uInvViewProj * vec4( vUV * 2.0 - 1.0, z * 2.0 - 1.0, 1.0 );
		tEnd   = length( s.xyz / s.w - uCamPos );
	}
	// enter the box (slabs); a camera inside starts at 0
	vec3 inv = 1.0 / ( rd + vec3( 1e-7 ) );
	vec3 t0  = ( uBoxC - uBoxH - uCamPos ) * inv, t1 = ( uBoxC + uBoxH - uCamPos ) * inv;
	vec3 tn  = min( t0, t1 ), tx = max( t0, t1 );
	float ta = max( max( tn.x, tn.y ), max( tn.z, 0.0 ) ), tb = min( min( tx.x, tx.y ), tx.z );
	if( tb <= ta )
	{
		fragColor = vec4( 0.0 );
		return;
	}
	vec3 sz  = uBoxH * 2.0;
	vec3 L   = vec3( 0.0 );
	float t  = ta;
	float maxT = min( tEnd, ta + length( sz ) * float( uBounces + 2 ) * 1.2 );
	for( int i = 0; i < 160 && t < maxT; ++i )
	{
		vec3 p    = uCamPos + rd * t;
		vec3 q    = ( p - ( uBoxC - uBoxH ) ) / sz;
		vec3 cell = floor( q );
		float b   = abs( cell.x ) + abs( cell.y ) + abs( cell.z );
		bool live = uShape == 1 || ( uShape == 2 && cell.y == 0.0 ) || ( uShape == 3 && cell.x == 0.0 && cell.y == 0.0 );
		if( !live || b > float( uBounces ) + 0.5 )
			break;
		vec3 st  = Strip( q, sz );
		float w  = max( uWidth, uPixAng * t * 0.7 );
		float d  = max( st.x - uWidth, 0.0 );
		float k  = exp( -d * d / ( w * w ) ) * ( uWidth / w ) + 0.06 * exp( -d / ( 6.0 * w ) );
		// the strip's colour runs along it (a chase); odd cells are mirrored: it runs back there
		float u  = st.y;
		vec3 col = uRainbow == 1 ? Hue( u * 0.5 + st.z * 0.33 + uPhase ) : vec3( 1.0 );
		float ch = 0.65 + 0.35 * sin( 6.2831853 * ( u * 2.0 - uPhase ) );
		float step_ = max( d * 0.7, w * 0.6 );
		L += uLed * col * ch * pow( uTint, vec3( b ) ) * k * min( step_ / w, 1.0 ) * 0.6;
		t += step_;
	}
	fragColor = vec4( ToDisplay( L ), 0.0 );
}
)GLSL";
}
