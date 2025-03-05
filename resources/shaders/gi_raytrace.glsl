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
    vec3 sceneNormalsBuffer[];
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

vec2 rnd_unit_vec2(vec2 p)
{
    float theta = hash01(p) * 2*PI;
    return vec2(cos(theta), sin(theta));
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

void main() 
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    
    // generate ray
    vec2 jitter = rnd_unit_vec2(vec2(samplesCurr, samplesCurr)) * 1.0;
    vec2 origin = vec2(uv) + jitter;
    vec2 seed = uv + hash(vec2(samplesCurr, samplesCurr));
    vec2 dir = rnd_unit_vec2(seed);
    
    float totalDist = 0.0;
    int steps = 0;
    bool hit = false;
    
    for (int i = 0; i < MAX_STEPS; i++) 
    {
        vec2 pos = origin + dir * totalDist;
        
        if(out_of_bound(ivec2(pos)))
            break;

        float d = sceneSdfBuffer[getIdx(ivec2(pos))];
        
        // Hit
        if (d < EPSILON) 
        {
            hit = true;
            steps = i;
            break;
        }
        
        totalDist += d;
        if (totalDist > INF) 
            break;
    }
    
    vec3 contribution = vec3(0);
    float lightIntensity = 1;

    if(hit)
    {
        contribution = lightIntensity * vec3(calc_attenuation(totalDist, 1./float(resolution.x), 1./float(resolution.x)));
    }
    // vec3 sampleColor = vec3(hash3(vec3(uv.x, uv.y, samplesCurr)));
    // contribution *= sampleColor;

    if(samplesCurr > 0)
    {
        contribution = (vec3(contribution) + (sceneGiBufferB[getIdx(uv)] * float(samplesCurr-1))) / float(samplesCurr);
    }

    sceneGiBufferA[getIdx(uv)] = contribution;
    // sceneGiBufferA[getIdx(uv)] = vec3(1.);
}
