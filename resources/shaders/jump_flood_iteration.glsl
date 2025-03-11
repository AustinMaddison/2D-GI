#version 430

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(std430, binding = 1) readonly buffer jfaLayout_A {
    ivec2 jfaBuffer_A[];
};

layout(std430, binding = 2) buffer jfaLayout_B {
    ivec2 jfaBuffer_B[];
};

uniform ivec2 resolution;
uniform int stepWidth;

#define getIdx(uv) ((uv).x + resolution.x * (uv).y)

bool out_of_bound(ivec2 p) {
    return p.x < 0 || p.y < 0 || p.x >= resolution.x || p.y >= resolution.y;
}

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    if (out_of_bound(uv)) {
        return;
    }
    uint idx = getIdx(uv);

    ivec2 best_seed = jfaBuffer_A[idx];
    ivec2 current_seed = best_seed;
    int best_dist_sq = 999999999;

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            // Generate offsets from -1 to +1 in both axes
            int dx = x - 1;
            int dy = y - 1;
            ivec2 samplePos = uv + ivec2(dx, dy) * stepWidth;

            if (out_of_bound(samplePos)) {
                continue;
            }

            // Get neighbor's seed from previous JFA pass
            ivec2 neighbor_seed = jfaBuffer_A[getIdx(samplePos)];
            
            // Calculate squared distance from current pixel to neighbor's seed
            int dist_sq = (uv.x +resolution.x - neighbor_seed.x) * (uv.x +resolution.x - neighbor_seed.x) 
                        + (uv.y - neighbor_seed.y) * (uv.y - neighbor_seed.y);

            // Update best seed if closer
            if (dist_sq < best_dist_sq) {
                best_dist_sq = dist_sq;
                best_seed = neighbor_seed;
            }
     

        }
    }

    // Write the closest seed to the output buffer
    jfaBuffer_B[idx] = best_seed;
}