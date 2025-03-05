#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer sceneSdfLayout
{
    vec3 sceneSdfBuffer[];
};

layout(std430, binding = 2) writeonly buffer sceneNormalsLayout
{
    vec3 sceneNormalsBuffer[];
};

uniform ivec2 resolution;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)


void main() 
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    sceneNormalsBuffer[getIdx(uv)] = vec3(1.0);
}
