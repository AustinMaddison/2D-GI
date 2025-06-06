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

uniform ivec2 uResolution;
uniform uint samples;

#define getIdx(st) (st.x)+uResolution.x*(st.y)

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

vec3 LinearToSRGB(vec3 linearRGB)
{
	bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
	vec3 higher = vec3(1.055)*pow(linearRGB, vec3(1.0/2.4)) - vec3(0.055);
	vec3 lower = linearRGB * vec3(12.92);

	return mix(higher, lower, cutoff);
}

vec3 LinearToRec709(vec3 linearColor) {
    return mix(
        pow(linearColor, vec3(1.0 / 2.4)) * 1.099 - 0.099, 
        linearColor * 4.5, 
        step(linearColor, vec3(0.018))
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
    
    float white = 8.;
    float whiteScale = 1.0 / (((white*(A*white + C*B))/(white*(A*white + B) + D*F)) - E/F);
    
    return mapped * whiteScale;
}
#define MAX_BOUNCE 8

void main() 
{
    vec2 st = gl_GlobalInvocationID.xy;
    vec2 texelSize = 1. / vec2(uResolution);
    uint idx = getIdx(ivec2(st));

    vec3 col = mix(sceneGiBuffer[idx] / float(MAX_BOUNCE), texture(colorMaskTex, st * texelSize).rgb * float(MAX_BOUNCE),  texture(colorMaskTex, st * texelSize).a);
    // vec3 col = sceneGiBuffer[idx] / float(MAX_BOUNCE);
    col = ToneMap_Uncharted2(col, 1.);
    col = LinearToSRGB(col);
    // col = LinearToRec709(col);

    finalPassBuffer[idx] = col;
}
