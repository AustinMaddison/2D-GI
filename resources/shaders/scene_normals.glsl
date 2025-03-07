#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer sceneSdfLayout
{
    float sceneSdfBuffer[];
};

layout(std430, binding = 2) writeonly buffer sceneNormalsLayout
{
    vec2 sceneNormalsBuffer[];
};

uniform ivec2 resolution;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

// Calculate normal using central differences
vec2 getNormal(ivec2 uv)
{
    // Calculate the SDF gradients in the X and Y directions using central differences
    uint dx_p0 = getIdx((uv + ivec2(1, 0)));
    uint dy_p0 = getIdx((uv + ivec2(0, 1)));
    uint dx_p1 = getIdx((uv - ivec2(1, 0)));
    uint dy_p2 = getIdx((uv - ivec2(0, 1)));

    float sdf_dx = sceneSdfBuffer[dx_p0] - sceneSdfBuffer[dx_p1];
    float sdf_dy = sceneSdfBuffer[dy_p0] - sceneSdfBuffer[dy_p2];

    return normalize(vec2(sdf_dx, sdf_dy));
}

void main() 
{
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);

    // Define weights for smoothing the normals from neighboring pixels
    const float diag_w = sqrt(2.0) / (4.0 + 4.0 * sqrt(2.0));  // Diagonal weight
    const float w = 1.0 / (4.0 + 4.0 * sqrt(2.0));  // Axis-aligned weight

    vec2 normal = vec2(0.0);

    // Sum up the normals from neighboring pixels with weights
    normal += getNormal(uv + ivec2(1, 0)) * w;  // Right
    normal += getNormal(uv - ivec2(1, 0)) * w;  // Left
    normal += getNormal(uv + ivec2(0, 1)) * w;  // Down
    normal += getNormal(uv - ivec2(0, 1)) * w;  // Up

    normal += getNormal(uv + ivec2(1, 1)) * diag_w;   // Bottom-right diagonal
    normal += getNormal(uv + ivec2(-1, 1)) * diag_w;  // Bottom-left diagonal
    normal += getNormal(uv + ivec2(1, -1)) * diag_w;  // Top-right diagonal
    normal += getNormal(uv + ivec2(-1, -1)) * diag_w; // Top-left diagonal

    // Normalize the resulting normal to ensure it's unit length
    sceneNormalsBuffer[getIdx(uv)] = normalize(normal);
}
