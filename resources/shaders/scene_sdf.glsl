#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer sceneMaskLayout
{
    float sceneMaskBuffer[];   
};

layout(std430, binding = 2) writeonly buffer sceneSdfLayout
{
    float sceneSdfBuffer[];   
};

uniform ivec2 resolution;
uniform uint samples;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

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

// root
float smin( float a, float b, float k )
{
    k *= 2.0;
    float x = (b-a)/k;
    float g = 0.5*(x+sqrt(x*x+1.0));
    return b - k * g;
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
#define INF 1e6

float sdf_map(vec2 uv)
{
    float map = INF;

    // walls
    float thickness = 5.;

    // circle
    float radius = 5.0f;
    float spacing = 150.0f;
    float angle = 0.0;
    float angleIncrement = .1;
    float radiusIncrement = 10.0;
    float currentRadius = radius;
    // vec2 center = vec2(resolution.x / 2.0);

    for (float i = 0.0; i < 100.0; i++) {
        vec2 center = vec2(resolution.x / 2.0) + vec2(cos(angle), sin(angle)) * currentRadius;

        map = min(map, sdCircle(uv - center - ivec2(200, 0), radius));
        angle += angleIncrement;
        currentRadius += radiusIncrement;

    }
    // vec2 center = vec2(resolution.x / 2.0);
    // map = min(map, sdCircle(uv - center, 100.));



    // float step_x = resolution.x / 60.;
    // float step_y = resolution.y / 60.;

    // for (float i = 0.0; i < 60.0; i++) {

    //     vec2 p = vec2(step_x * i, step_y * i);
    //     vec2 center = p;
    //     map = min(map, sdCircle(uv - center, radius));
    // }

    // random walls
    float wallThickness = 1.0;
    float wallLength = 200.0;
    float numWalls = 10.0;
    for (float i = 0.0; i < numWalls; i++) {
        vec2 start = vec2(rand(vec2(i, 0.0)) * resolution.x, rand(vec2(i, 1.0)) * resolution.x);
        vec2 end = start + vec2(rand(vec2(i, 2.0)) * wallLength, rand(vec2(i, 3.0)) * wallLength);
        map = min(map, sdSegment(uv+vec2(100.), start, end) - wallThickness);
    }

    // map = min(map, sdSegment(uv, vec2(464., 82.), vec2(734., 342.)) - wallThickness);

    // map = min(map, sdSegment(uv, vec2(487., 259.), vec2(624., 417.)) - wallThickness);

    
    return map;
}

void main()
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    sceneSdfBuffer[getIdx(uv)] = sdf_map(uv);
    // sceneSdfBuffer[getIdx(uv)] = 1.;
}
