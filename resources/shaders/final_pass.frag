#version 430

layout(std430, binding = 1) readonly buffer finalPassLayout
{
    vec3 finalPassBuffer[];
};

in vec2 fragTexCoord;
out vec4 fragColor;

uniform vec2 resolution;

#define getIdx(uv) (uv.x)+resolution*(uv.y)

void main()
{
    ivec2 uv = ivec2(fragTexCoord*resolution);
    uint idx = getIdx(uv);

    fragColor = vec4(finalPassBuffer[uv], 1.0f);
}
