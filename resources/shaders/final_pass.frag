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

vec3 gammaCorrect(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0f, 1.0f);
}

vec3 LessThan(vec3 f, float value)
{
    return vec3(
        (f.x < value) ? 1.0f : 0.0f,
        (f.y < value) ? 1.0f : 0.0f,
        (f.z < value) ? 1.0f : 0.0f);
}

vec3 LinearToSRGB(vec3 rgb)
{
    rgb = clamp(rgb, 0.0f, 1.0f);
    
    return mix(
        pow(rgb * 1.055f, vec3(1.f / 2.4f)) - 0.055f,
        rgb * 12.92f,
        LessThan(rgb, 0.0031308f)
    );
}

void main()
{
    ivec2 uv = ivec2(fragTexCoord*resolution);

    // finalColor = vec4(renderSDF(getMap(uv)), 1.0f);
    // vec3 col = getGi(uv) * renderSDF(getMap(uv));
    vec3 col = getGi(uv);
    col = ACESFilm(col);
    col = LinearToSRGB(col);

    finalColor = vec4(col, 1.0f);
}
