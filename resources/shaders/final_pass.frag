#version 430

layout(std430, binding = 1) readonly buffer finalPassLayout
{
    vec3 finalPassBuffer[];
};

layout(binding = 2) uniform sampler2D colorMaskTex;
layout(binding = 3) uniform sampler2D sdfTex;
layout(binding = 4) uniform sampler2D normalsTex;
layout(binding = 5) uniform sampler2D jfaTex;

in vec2 fragTexCoord;
out vec4 fragColor;

uniform ivec2 uResolution;
uniform float uTime;

#define getIdx(uv) (uv.x)+uResolution.x*(uv.y)

ivec2 flipUV(ivec2 uv, ivec2 uResolution) 
{
    return ivec2(uv.x, uResolution.y - uv.y - 1);
}

void main()
{
    ivec2 uv = ivec2(fragTexCoord*uResolution);
    uint idx = getIdx(uv);

    vec3 col;

    // fragColor = vec4(fragTexCoord, 0., 1.0f);
    // col = texture(colorMaskTex, fragTexCoord).rgb;
    // col = vec3(texture(sdfTex, fragTexCoord)/float(uResolution.x));
    // col = vec3(texture(normalsTex, fragTexCoord).rg, 0.);
    // col = vec3(finalPassBuffer[getIdx(uv)]);
    // float sdf = smoothstep(1.0f - (sin(uTime)*.5+0.5), -0.1f, length(texture(jfaTex, fragTexCoord).rg - fragTexCoord));
    // float sdf = smoothstep(1., -0.1f, length(texture(jfaTex, fragTexCoord).rg - fragTexCoord));
    // col = vec3(texture(jfaTex, fragTexCoord).rg, 0.);
    // col = vec3(sdf);

    col = vec3(finalPassBuffer[getIdx(uv)]);
    fragColor = vec4(col, 1.0f);
}
