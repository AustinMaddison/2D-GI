#version 430

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D colorMaskTex;
layout(binding = 2, r32f) uniform image2D sdfImage;
layout(binding = 3) uniform sampler2D jfaTex;

uniform ivec2 resolution;

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

// SDF Functions
// source: https://iquilezles.org/articles/distfunctions2d/
/* -------------------------------------------------------------------------- */

float sphereSDF(vec2 p, float r) {
    return length(p) - r;
}

float boxSDF(vec2 p, vec2 size) {
    vec2 d = abs(p) - size;
    return min(max(d.x, d.y), 0.) + length(max(d, vec2(0, 0)));
}

float segmentSDF(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

void AddObj(inout float dist, inout vec3 color, float d, vec3 c) {
    if (dist > d) {
        dist = d;
        color = c;
    }
}

void Scene(in vec2 pos, out vec3 color, out float dist) {
    dist = INF;
    color = vec3(0, 0, 0);

    vec2 center = vec2(0.0);
 
    AddObj(dist, color, segmentSDF(
        pos, 
        center + vec2(0.0, 5.0), 
        center + vec2(0.0, -5.)) - 0.02, 
        vec3(1.0, 1.0, 10.0)
    );

    AddObj(dist, color, segmentSDF(
        pos, 
        center + vec2(5.0, 0.0), 
        center + vec2(-5.0, 0.)) - 0.02, 
        vec3(10.0, 1.0, 0.0)
    );

    AddObj(dist, color, sphereSDF(
        pos - vec2(5.0, 5.0), 
        1.), 
        vec3(5.0, 5.0, 5.0)
    );


    AddObj(dist, color, sphereSDF(
        pos - vec2(-2.5, 2.5), 
        1.), 
        vec3(0.0, 0.0, 0.0)
    );

    AddObj(dist, color, segmentSDF(
        pos, 
        center + vec2(5.0, 2.5), 
        center + vec2(1.0, 2.5)) - 0.04, 
        vec3(0.0, 0.0, 0.0)
    );
}

vec2 GetSceneNormal(vec2 pos) {
    float h = EPSILON;
    float distX1, distX2, distY1, distY2;
    vec3 color;

    Scene(pos + vec2(h, 0), color, distX1);
    Scene(pos - vec2(h, 0), color, distX2);
    Scene(pos + vec2(0, h), color, distY1);
    Scene(pos - vec2(0, h), color, distY2);

    vec2 normal = vec2(distX1 - distX2, distY1 - distY2);
    return normalize(normal);
}

void main()
{

    ivec2 st = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = (vec2(st) + 0.5) / vec2(resolution);
    uv = uv * 2.0 - 1.0;
    uv.x *= float(resolution.x) / float(resolution.y);


    imageStore(sdfImage, uv, vec4(sdf_map(uv)));
}
