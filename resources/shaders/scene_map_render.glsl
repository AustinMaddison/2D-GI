#version 430

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;

// Output fragment color
out vec4 finalColor;

layout(std430, binding = 1) readonly buffer mapLayout
{
    float mapBuffer[];
};

// Output resolution
uniform vec2 resolution;

#define getMap(uv) mapBuffer[uv.x + uv.y*uvec2(resolution).x]

void main()
{
    ivec2 uv = ivec2(fragTexCoord*resolution);
    finalColor = vec4(vec3(getMap(uv)), 1.0f);
}
