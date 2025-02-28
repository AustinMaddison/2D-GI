#version 430
#define MAP_WIDTH 768 // multiple of 16

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
uniform vec2 mouse;

#define getMap(uv) mapBuffer[uv.x + uv.y*uvec2(resolution).x]

void main()
{
    ivec2 uv = ivec2(fragTexCoord*resolution);
    finalColor = vec4(vec3(sin(getMap(uv)/MAP_WIDTH*100.0f)), 1.0f);
}
