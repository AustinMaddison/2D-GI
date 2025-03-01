#version 430

#define MAP_WIDTH 768 // multiple of 16
#define INF 1e6
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

// https://thebookofshaders.com/edit.php#11/2d-gnoise.frag

#define FK(k) floatBitsToInt(cos(k))^floatBitsToInt(k)

float hash(float a, float b) {
    int x = FK(a); int y = FK(b);
    return float((x*x+y)*(y*y-x)+x)/2.14e9;
}

vec2 random2(vec2 uv)
{
    float h1 = hash(uv.x, uv.y);
    float h2 = hash(h1, uv.x + uv.y);
    return vec2(h1,h2);
}

vec2 random2(float x)
{
    float h1 = hash(x, x*x);
    float h2 = hash(h1, x);
    return vec2(h1,h2);
}

void main() 
{
  ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    
    vec2 origin = vec2(uv);
    
    vec2 dir = normalize(random2(vec2(uv) + random2(float(samples))));
    
    float totalDist = 0.0;
    int steps = 0;
    bool hit = false;
    
    for (int i = 0; i < MAX_STEPS; i++) {
        vec2 pos = origin + dir * totalDist;
        ivec2 posInt = ivec2(pos);
        
        if (posInt.x < 0) {
            break;
        }

        if (posInt.x >= MAP_WIDTH) {
            break;
        }

        if (posInt.y < 0) {
            break;
        }

        if (posInt.y >= MAP_WIDTH) {
            break;
        }
        
        float d = getMap(posInt);
        
        if (d < EPSILON) {
            hit = true;
            steps = i;
            break;
        }
        
        totalDist += d;
        if (totalDist > INF) {
            break;
        }
    }
    
    float radiance = hit ? exp(20.0f/float(totalDist)) : 0.0;
    radiance -= 1.;

    vec3 giColor;
    if(samples < 1)
    {
        giColor = (vec3(radiance));
    }
    else{
        giColor = (vec3(radiance) + (getGiB(uv)*float(samples-1)))/float(samples);
    }

    atomicAdd(genRayCount, 1);
    
    // Write the computed GI value to the destination buffer for this pixel
    setGiA(uv, giColor);
    // setGiA(uv, vec3(giColor));
    // setGiA(uv, getGiB(uv));
}
