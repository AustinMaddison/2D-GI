#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D colorMaskTex;

layout(std430, binding = 2) readonly buffer sceneGiLayout
{
    vec3 sceneGiBuffer[];
};

layout(std430, binding = 3) writeonly buffer finalPassLayout 
{
    vec3 finalPassBuffer[];   
};

uniform ivec2 resolution;
uniform uint samples;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

vec3 gammaCorrect(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
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

vec3 ToneMap_Uncharted2(vec3 color, float exposure)
{
    color *= exposure;
    
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    
    vec3 mapped = ((color*(A*color + C*B) + D*E) / (color*(A*color + B) + D*F)) - E/F;
    
    float white = 11.2;
    float whiteScale = 1.0 / (((white*(A*white + C*B))/(white*(A*white + B) + D*F)) - E/F);
    
    return mapped * whiteScale;
}

void main() 
{
    vec2 uv = gl_GlobalInvocationID.xy;
    uint idx = getIdx(ivec2(uv));

    vec3 col = sceneGiBuffer[idx];
    col = ToneMap_Uncharted2(col, .5);
    col = LinearToSRGB(col);

    finalPassBuffer[idx] = col;
}
