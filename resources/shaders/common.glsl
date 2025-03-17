#define INF 1e6
#define PI 3.14159265359
#define getIdx(uv) (uv.x)+uResolution.x*(uv.y)

// Random Numbers
// https://suricrasia.online/blog/shader-functions/
#define FK(k) floatBitsToInt(cos(k))^floatBitsToInt(k)
float hash(vec2 p) {
    int x = FK(p.x); int y = FK(p.y);
    return float((x*x+y)*(y*y-x)+x)/2.14e9;
}

float hash01(vec2 p) {
    return fract(sin(dot(p.xy, vec2(12.9898,78.233)))*43758.5453123);
}

vec2 rnd_unit_circle(vec2 p)
{
    float theta = hash01(p) * 2*PI;
    return vec2(cos(theta), sin(theta));
}

vec2 rnd_unit_half_circle(vec2 normal, vec2 p)
{
    vec2 tangent = vec2(-normal.y, normal.x); 
    float theta = (hash01(p) * PI) - PI*.5;
    return cos(theta) * normal + sin(theta) * tangent;
}

bool out_of_bound(ivec2 p)
{
    if (p.x < 0) return true;
    if (p.y < 0) return true;
    if (p.x >= uResolution.x) return true;
    if (p.y >= uResolution.y) return true;
    return false;
}

// SDF Functions
// source: https://iquilezles.org/articles/distfunctions2d/
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

