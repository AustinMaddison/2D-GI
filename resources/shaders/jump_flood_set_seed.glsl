#version 430

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1, rgba32f) uniform image2D sceneColorMaskImg;
layout(binding = 2, rgba32f) uniform image2D jfaSeedImg;

uniform ivec2 uResolution;
#define getIdx(uv) (uv.x)+uResolution.x*(uv.y) 
#define INF 1E9

void main()
{
    ivec2 st = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = st / vec2(uResolution);
    // uv = (floor(uv * uResolution) + 0.5) / uResolution;
    float mask = imageLoad(sceneColorMaskImg, st).a;

    imageStore(jfaSeedImg, st, vec4(vec3(uv, 1.0f) * mask, 0.0));
}
