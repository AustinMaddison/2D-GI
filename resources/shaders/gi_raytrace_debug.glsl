#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer sceneColorLayout 
{
    vec3 sceneColorBuffer[];   
};

layout(std430, binding = 2) readonly buffer sceneSdfLayout
{
    float sceneSdfBuffer[];
};

layout(std430, binding = 3) readonly buffer sceneNormalsLayout
{
    vec2 sceneNormalsBuffer[];
};

layout(std430, binding = 4) writeonly buffer sceneGiALayout 
{
    vec3 sceneGiBufferA[];   
};

layout(std430, binding = 5) readonly buffer sceneGiBLayout 
{
    vec3 sceneGiBufferB[];   
};

uniform ivec2 uResolution;
uniform ivec2 uMousePos;
uniform uint uSamples;

#define INF 1e10
#define PI 3.14159265359
#define EPSILON 0.01
#define MAX_STEPS 100
#define MAX_BOUNCE 5

#define getIdx(uv) (uv.x)+uResolution.x*(uv.y)

// https://suricrasia.online/blog/shader-functions/
#define FK(k) floatBitsToInt(cos(k))^floatBitsToInt(k)
float hash(vec2 p) {
    int x = FK(p.x); int y = FK(p.y);
    return float((x*x+y)*(y*y-x)+x)/2.14e9;
}

float hash01(vec2 p) {
    return fract(sin(dot(p.xy, vec2(12.9898,78.233)))*43758.5453123);
}

vec2 rnd_unit_vec2(vec2 p)
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

// https://learnwebgl.brown37.net/09_lights/lights_attenuation.html
float calc_attenuation(float d, float c1, float c2)
{
    return clamp(1.0 / (1.0 + c1*d + c1*d*d), 0.0, 1.0);
}

float calc_attenuation_simple(float d)
{
    return clamp(1.0 / (d*d), 0.0, 1.0);
}

float sdCircle( vec2 p, float r )
{
    return length(p) - r;
}

float sdSegment( in vec2 p, in vec2 a, in vec2 b )
{
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

void main() 
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    
    // generate ray
    vec2 origin = vec2(uResolution.x/2., uResolution.y/2.);
    vec2 seed = origin + hash(vec2(uSamples, uSamples));
    vec2 dir = rnd_unit_vec2(seed);

    float lineSdf = 10E6;
    float lineMissSdf = 10E6;
    float totalDist = 0.0;

    //
    for (int b = 0; b < MAX_BOUNCE; b++)
    {
        bool hit = false;
        vec2 pos = origin;
        // totalDist = 0;


        for (int i = 0; i < MAX_STEPS; i++) 
        {
            float d = sceneSdfBuffer[getIdx(ivec2(pos))];
            pos +=  dir * d;

            if(out_of_bound(ivec2(pos)))
                break;

            // Hit
            if (d < EPSILON) 
            {
                hit = true;
                break;
            }
            
            totalDist += d;
            if (totalDist > INF) 
                break;
        }

        vec2 trace_pos = pos;
        vec2 trace_origin = origin;

        if(hit)
        {
            // vec3 color = sceneColorBuffer[getIdx(ivec2(pos))];
            vec2 normals = sceneNormalsBuffer[getIdx(ivec2(pos -dir *5.))];

            dir = rnd_unit_half_circle(normals, seed);
            origin = pos + dir;

            // trace_sdf = min(trace_sdf, sdCircle(vec2(uv)-p2, 2.));
            // trace_sdf = min(trace_sdf, sdCircle(vec2(uv)-p2, 2.));

            lineSdf = min(lineSdf, sdSegment(vec2(uv), trace_origin, trace_pos));
            lineSdf = min(lineSdf,sdCircle(vec2(uv)-trace_origin, 2.));
            lineSdf = min(lineSdf,sdCircle(vec2(uv)-trace_pos, 2.));
            // Draw debug hit points.
            // sceneGiBufferA[getIdx(uv)] = vec3(0., 0., 0.f);
            // sceneGiBufferA[getIdx(ivec2(pos))] = vec3(1., 0., 0.f);
        }
        else {
            lineMissSdf = min(lineMissSdf, sdSegment(vec2(uv), trace_origin, trace_pos));
            lineMissSdf = min(lineMissSdf,sdCircle(vec2(uv)-trace_origin, 2.));
            lineMissSdf = min(lineMissSdf,sdCircle(vec2(uv)-trace_pos, 2.));
        }

 
    }

    lineSdf = smoothstep(1., 0., (lineSdf));
    lineMissSdf = smoothstep(1., 0., (lineMissSdf));

    float map = smoothstep(1., 0., (sceneSdfBuffer[getIdx(uv)]));
    vec3 contribution = mix(vec3(0.5, 0.5, 0.5) * map, vec3(0.8,0.,0.), lineSdf);

    contribution +=  vec3(1.)* lineMissSdf;
    
    // if(uSamples > 0)
    // {
    //     contribution = (vec3(contribution) + (sceneGiBufferB[getIdx(uv)] * float(uSamples-1))) / float(uSamples);
    // }

    sceneGiBufferA[getIdx(uv)] = + sceneGiBufferB[getIdx(uv)] + contribution;

    // sceneGiBufferA[getIdx(uv)] = vec3(sceneNormalsBuffer[getIdx(uv)], 0.);
    // sceneGiBufferA[getIdx(uv)] = vec3(1.0);
}
