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

layout(std430, binding = 3) readonly buffer giLayout
{
    vec3 giBuffer[];
};

// Output resolution
uniform vec2 resolution;

#define getMap(uv) mapBuffer[uv.x + uv.y*uvec2(resolution).x]
#define getGi(uv) giBuffer[uv.x + uv.y*uvec2(resolution).x]


vec3 renderSDF(float sdf)
{
    sdf = sdf/MAP_WIDTH;

    vec3 col = vec3(1.0) - sign(sdf)*vec3(0.9,0.9,0.9);
	col *= 0.8 + 0.2 * cos(sdf* 600.0);
	col = mix(col, vec3(1.0), 1.0-smoothstep(0.0,25E-4,abs(sdf)) );
    return vec3(col);
}

void main()
{
    ivec2 uv = ivec2(fragTexCoord*resolution);

    // finalColor = vec4(renderSDF(getMap(uv)), 1.0f);
    // vec3 col = getGi(uv) * renderSDF(getMap(uv));
    vec3 col = getGi(uv);
    finalColor = vec4(col, 1.0f);
}
