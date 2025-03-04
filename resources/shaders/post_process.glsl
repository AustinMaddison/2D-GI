#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer sceneGiLayout
{
    vec3 sceneGiBuffer[];
};

layout(std430, binding = 2) readonly buffer sceneColorLayout 
{
    vec3 sceneColorBuffer[];   
};

layout(std430, binding = 3) writeonly buffer finalPassLayout 
{
    vec3 finalPassBuffer[];   
};

in uvec3 gl_NumWorkGroups;
in uvec3 gl_WorkGroupID;
in uvec3 gl_LocalInvocationID;
in uvec3 gl_GlobalInvocationID;
in uint  gl_LocalInvocationIndex;

uniform ivec2 resolution;
uniform uint samples;

#define getIdx(uv) (uv.x)+resolution*(uv.y)

vec3 gammaCorrect(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0f, 1.0f);
}

vec3 LessThan(vec3 f, float value)
{
    return vec3(
        (f.x < value) ? 1.0f : 0.0f,
        (f.y < value) ? 1.0f : 0.0f,
        (f.z < value) ? 1.0f : 0.0f);
}

vec3 LinearToSRGB(vec3 rgb)
{
    rgb = clamp(rgb, 0.0f, 1.0f);
    
    return mix(
        pow(rgb * 1.055f, vec3(1.f / 2.4f)) - 0.055f,
        rgb * 12.92f,
        LessThan(rgb, 0.0031308f)
    );
}

void main() 
{
    vec2 uv = gl_GlobalInvocationID.xy;
    uint idx = getIdx(uv);

    vec3 col = sceneGiBuffer[idx];
    col = ACESFilm(col);
    col = LinearToSRGB(col);

    finalPassBuffer[idx] = col;
}
