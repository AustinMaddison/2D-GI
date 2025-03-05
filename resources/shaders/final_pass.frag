#version 430

layout(std430, binding = 1) readonly buffer finalPassLayout
{
    vec3 finalPassBuffer[];
};

layout(std430, binding = 2) readonly buffer sceneSdfLayout
{
    float sceneSdfBuffer[];
};

in vec2 fragTexCoord;
out vec4 fragColor;

uniform ivec2 resolution;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

ivec2 flipUV(ivec2 uv, ivec2 resolution) {
    return ivec2(uv.x, resolution.y - uv.y - 1);
}

void main()
{
    ivec2 uv = ivec2(fragTexCoord*resolution);
    uint idx = getIdx(uv);

    fragColor = vec4(finalPassBuffer[idx], 1.0f);
    // fragColor = vec4(vec3(sceneSdfBuffer[idx]), 1.0f);
    // fragColor = vec4(vec3(uv, 0.), 1.0f);
    // fragColor = vec4(vec3(uv, 0.0), 1.0f);
    // fragColor = vec4(vec3(fragTexCoord, 0.0), 1.0f);
}
