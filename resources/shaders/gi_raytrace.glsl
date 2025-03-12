#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D sdfTex;
layout(binding = 2) uniform sampler2D normalsTex;

layout(std430, binding = 3) buffer rayCountLayout
{
    uint rayCountBuffer;
};

layout(std430, binding = 4) writeonly buffer sceneGiALayout 
{
    vec3 sceneGiBufferA[];   
};

layout(std430, binding = 5) readonly buffer sceneGiBLayout 
{
    vec3 sceneGiBufferB[];   
};

uniform ivec2 resolution;
uniform uint samplesCurr;

#define INF 1e6
#define PI 3.14159265359
#define EPSILON 0.01
#define MAX_STEPS 100
#define MAX_BOUNCE 3

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

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
    if (p.x >= resolution.x) return true;
    if (p.y >= resolution.y) return true;
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

void main() 
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    
    // generate ray
    vec2 jitter = rnd_unit_circle(vec2(samplesCurr, samplesCurr)) * 1.0;
    vec2 origin = vec2(uv) + jitter;
    vec2 seed = uv + hash(vec2(samplesCurr, samplesCurr));
    vec2 dir = rnd_unit_circle(seed);
    
    float totalDist = 0.0;
    vec3 contribution = vec3(0.0f);
    float scale = resolution.x;
    float lightIntensity = 0.33;
    
    for (int b = 0; b < MAX_BOUNCE; b++)
    {
        bool hit = false;
        vec2 pos = origin;
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
        if(hit)
        {
            vec3 color = vec3(1.);
            vec2 normals = sceneNormalsBuffer[getIdx(ivec2(pos -dir *5.))];
            float energy = calc_attenuation(totalDist/scale, 2., 10.);

            contribution += color * lightIntensity * energy;
            dir = rnd_unit_half_circle(normals, seed);
            origin = pos + dir;
        }
    }
    
    if(samplesCurr > 0)
    {
        contribution = (vec3(contribution) + (sceneGiBufferB[getIdx(uv)] * float(samplesCurr-1))) / float(samplesCurr);
    }

    sceneGiBufferA[getIdx(uv)] = contribution;
    atomicAdd(rayCountBuffer, 1);
    // sceneGiBufferA[getIdx(uv)] = vec3(sceneNormalsBuffer[getIdx(uv)], 0.);
    // sceneGiBufferA[getIdx(uv)] = vec3(1.0);
}
