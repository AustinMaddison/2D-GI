#version 430

#define MAP_WIDTH 768 // multiple of 16
#define INF 1e6
#define PI 3.14159265359
#define EPSILON 0.01
#define MAX_STEPS 100

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) buffer mapLayout
{
    float mapBuffer[];
};

layout(std430, binding = 2) buffer generatedRayCountLayout 
{
    uint genRayCount;   
};

layout(std430, binding = 3) writeonly buffer giALayout 
{
    vec3 giBufferA[];   
};

layout(std430, binding = 4) readonly buffer giBLayout 
{
    vec3 giBufferB[];   
};

uniform uint samples;

#define getMap(uv) mapBuffer[(uv.x) + MAP_WIDTH*(uv.y)]
#define setGiA(uv, value) giBufferA[(uv.x) + MAP_WIDTH*(uv.y)].rgb = value
#define getGiB(uv) giBufferB[(uv.x) + MAP_WIDTH*(uv.y)].rgb 
#define setMap(uv, value) mapBuffer[(uv.x) + MAP_WIDTH*(uv.y)] = value

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
    if (p.x >= MAP_WIDTH) return true;
    if (p.y >= MAP_WIDTH) return true;
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
    vec2 jitter = rnd_unit_vec2(vec2(samples, samples)) * 1.0;
    vec2 origin = vec2(uv) + jitter;
    vec2 seed = uv + hash(vec2(samples, samples));
    vec2 dir = rnd_unit_vec2(seed);
    
    float totalDist = 0.0;
    int steps = 0;
    bool hit = false;
    
    for (int i = 0; i < MAX_STEPS; i++) 
    {
        vec2 pos = origin + dir * totalDist;
        ivec2 posInt = ivec2(pos);
        
        if(out_of_bound(posInt))
            break;

        float d = getMap(posInt);
        
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
        contribution = lightIntensity * vec3(calc_attenuation(totalDist, 1./MAP_WIDTH, 1./MAP_WIDTH));
    }
    // vec3 sampleColor = vec3(hash3(vec3(uv.x, uv.y, samples)));
    // contribution *= sampleColor;

    if(samples > 0)
    {
        contribution = (vec3(contribution) + (getGiB(uv) * float(samples-1))) / float(samples);
    }

    atomicAdd(genRayCount, 1);
    setGiA(uv, contribution);
}
