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

void main()
{
    ivec2 coords = ivec2(fragTexCoord*resolution);
    finalColor = vec4(vec3(mapBuffer[coords.x + coords.y*uvec2(resolution).x]), 1.0f);
    // finalColor = vec4(fragTexCoord.x, fragTexCoord.y, 0.0f, 1.0f);

    // if ((mapBuffer[coords.x + coords.y*uvec2(resolution).x]) == 1) finalColor = vec4(1.0);
    // else finalColor = vec4(0.0, 0.0, 0.0, 1.0);
}
