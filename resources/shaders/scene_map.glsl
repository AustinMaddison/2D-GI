#version 430

#define MAP_WIDTH 768 // multiple of 16

// SDF Functions
// source: https://iquilezles.org/articles/distfunctions2d/
/* -------------------------------------------------------------------------- */
float sdCircle( vec2 p, float r )
{
    return length(p) - r;
}

float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

float sdOrientedBox( in vec2 p, in vec2 a, in vec2 b, float th )
{
    float l = length(b-a);
    vec2  d = (b-a)/l;
    vec2  q = (p-(a+b)*0.5);
          q = mat2(d.x,-d.y,d.y,d.x)*q;
          q = abs(q)-vec2(l,th)*0.5;
    return length(max(q,0.0)) + min(max(q.x,q.y),0.0);    
}

float sdSegment( in vec2 p, in vec2 a, in vec2 b )
{
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

float sdPie( in vec2 p, in vec2 c, in float r )
{
    p.x = abs(p.x);
    float l = length(p) - r;
    float m = length(p-c*clamp(dot(p,c),0.0,r)); // c=sin/cos of aperture
    return max(l,m*sign(c.y*p.x-c.x*p.y));
}

// Uitility functions
// float opOnion( in vec2 p, in float r )
// {
//   return abs(sdShape(p)) - r;
// }

// float opRound( in vec2 p, in float r )
// {
//   return sdShape(p) - r;
// }


// smin functions https://iquilezles.org/articles/smin/
/* -------------------------------------------------------------------------- */

// root
float smin( float a, float b, float k )
{
    k *= 2.0;
    float x = (b-a)/k;
    float g = 0.5*(x+sqrt(x*x+1.0));
    return b - k * g;
}

// // sigmoid
// float smin( float a, float b, float k )
// {
//     k *= log(2.0);
//     float x = (b-a)/k;
//     float g = x/(1.0-exp2(-x));
//     return b - k * g;
// }
// // quadratic polynomial
// float smin( float a, float b, float k )
// {
//     k *= 4.0;
//     float x = (b-a)/k;
//     float g = (x> 1.0) ? x :
//               (x<-1.0) ? 0.0 :
//               (x*(2.0+x)+1.0)/4.0;
//     return b - k * g;
// }
// // cubic polynomial
// float smin( float a, float b, float k )
// {
//     k *= 6.0;
//     float x = (b-a)/k;
//     float g = (x> 1.0) ? x :
//               (x<-1.0) ? 0.0 :
//               (1.0+3.0*x*(x+1.0)-abs(x*x*x))/6.0;
//     return b - k * g;
// }
// // quartic polynomial
// float smin( float a, float b, float k )
// {
//     k *= 16.0/3.0;
//     float x = (b-a)/k;
//     float g = (x> 1.0) ? x :
//               (x<-1.0) ? 0.0 :
//               (x+1.0)*(x+1.0)*(3.0-x*(x-2.0))/16.0;
//     return b - k * g;
// }
// // circular
// float smin( float a, float b, float k )
// {
//     k *= 1.0/(1.0-sqrt(0.5));
//     float x = (b-a)/k;
//     float g = (x> 1.0) ? x :
//               (x<-1.0) ? 0.0 :
//               1.0+0.5*(x-sqrt(2.0-x*x));
//     return b - k * g;
// }

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// layout(std430, binding = 1) readonly restrict buffer golLayout {
//     uint golBuffer[];       // golBuffer[x, y] = golBuffer[x + gl_NumWorkGroups.x * y]
// };

// layout(std430, binding = 2) writeonly restrict buffer golLayout2 {
//     uint golBufferDest[];   // golBufferDest[x, y] = golBufferDest[x + gl_NumWorkGroups.x * y]
// };

layout(std430, binding = 1) writeonly restrict buffer mapLayout {
    float mapBufferDest[];   // golBufferDest[x, y] = golBufferDest[x + gl_NumWorkGroups.x * y]
};


#define setMap(uv, value) mapBufferDest[(uv.x) + MAP_WIDTH*(uv.y)] = value

float sdf_map(vec2 uv)
{
    float map = 0.0f;

    vec2 p = vec2(MAP_WIDTH/2, MAP_WIDTH/2);
    map += sdCircle(uv - p, 100.0f);

    return map;
}

void main()
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);

    float map = sdf_map(uv);
    map = sin(map);

    setMap(uv, map);
    // setMap(uv, 1.0f);
}
