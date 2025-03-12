#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// layout(binding = 1) uniform sampler2D sdfTex;
layout(binding = 1, r32f) uniform image2D sdfImage;
layout(binding = 2, rgba32f) uniform image2D normalsImage;

uniform ivec2 resolution;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

vec2 getNormal(ivec2 uv)
{
    float sdf_c = imageLoad(sdfImage, uv).r;
    float sdf_r = imageLoad(sdfImage, uv + ivec2(1, 0)).r;
    float sdf_l = imageLoad(sdfImage, uv - ivec2(1, 0)).r;
    float sdf_t = imageLoad(sdfImage, uv + ivec2(0, 1)).r;
    float sdf_b = imageLoad(sdfImage, uv - ivec2(0, 1)).r;

    vec2 gradient = vec2(sdf_r - sdf_l, sdf_t - sdf_b) * 0.5;
    return normalize(gradient);
}

void main() 
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    vec2 texelSize = 1.0 / vec2(resolution);

    // extra samples to make smooth
    // const float diag_w = sqrt(2.0) / (4.0 + 4.0 * sqrt(2.0));  // Diagonal weight
    // const float w = 1.0 / (4.0 + 4.0 * sqrt(2.0));  // Axis-aligned weight

    vec2 normal= vec2(0.0);
    // normal += getNormal(uv + ivec2(1, 0)) * w;  // Right
    // normal += getNormal(uv - ivec2(1, 0)) * w;  // Left
    // normal += getNormal(uv + ivec2(0, 1)) * w;  // Down
    // normal += getNormal(uv - ivec2(0, 1)) * w;  // Up

    // normal += getNormal(uv + ivec2(1, 1)) * diag_w;   // Bottom-right diagonal
    // normal += getNormal(uv + ivec2(-1, 1)) * diag_w;  // Bottom-left diagonal
    // normal += getNormal(uv + ivec2(1, -1)) * diag_w;  // Top-right diagonal
    // normal += getNormal(uv + ivec2(-1, -1)) * diag_w; // Top-left diagonal

    normal = getNormal(uv);
    // normal = normalize(normal);
    // normal.xy = vec2(imageLoad(sdfImage, uv).r);
    imageStore(normalsImage, uv, vec4(normal, 0., 0.));
    // imageStore(normalsImage, uv, vec4(uv*texelSize, 0., 0.));
}
