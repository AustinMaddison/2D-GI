#version 430

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer sceneColorMaskLayout
{
    vec4 sceneColorMaskBuffer[];
};

layout(std430, binding = 2) writeonly buffer jfaLayout
{
    ivec2 jfaBuffer[];
};

uniform ivec2 resolution;
#define getIdx(uv) (uv.x)+resolution.x*(uv.y) 

void main()
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    uint idx = getIdx(uv);

    jfaBuffer[idx] = uv * int(floor(sceneColorMaskBuffer[idx].a));
}
