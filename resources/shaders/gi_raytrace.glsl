#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D sdfTex;
layout(binding = 2) uniform sampler2D normalsTex;

layout(std430, binding = 3) buffer rayCountLayout
{
    uint rayCountBuffer;
};

layout(std430, binding = 4) writeonly buffer sceneGiALayout
{
    vec3 sceneGiBufferA[];
};

layout(std430, binding = 5) readonly buffer sceneGiBLayout
{
    vec3 sceneGiBufferB[];
};

uniform ivec2 resolution;
// uniform ivec2 resolution;
uniform uint samplesCurr;
uniform float time;

#define INF 1e9
#define PI 3.14159265359
#define EPSILON 0.01
#define MAX_STEPS 100
#define MAX_BOUNCE 3

#define getIdx(uv) (uv.x)+resolution.x*(uv.y)

// https://suricrasia.online/blog/shader-functions/
#define FK(k) floatBitsToInt(cos(k))^floatBitsToInt(k)
float hash(vec2 p) {
    int x = FK(p.x);
    int y = FK(p.y);
    return float((x * x + y) * (y * y - x) + x) / 2.14e9;
}

float hash01(vec2 p) {
    return fract(sin(dot(p.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec2 RandomUnitCircleDir(vec2 p)
{
    float theta = hash01(p) * 2 * PI;
    return vec2(cos(theta), sin(theta));
}

vec2 RandomUnitHfCircleDir(vec2 normal, vec2 p)
{
    vec2 tangent = vec2(-normal.y, normal.x);
    float theta = (hash01(p) * PI) - PI * .5;
    return cos(theta) * normal + sin(theta) * tangent;
}

bool OutOfBounds(vec2 pos)
{
    vec2 bound = vec2(float(resolution.x) / float(resolution.y), 1.0) * 10.;
    if (pos.x < -bound.x) return true;
    if (pos.y < -bound.y) return true;
    if (pos.x >= bound.x) return true;
    if (pos.y >= bound.y) return true;
    return false;
}

// https://learnwebgl.brown37.net/09_lights/lights_attenuation.html
float Attenuation(float d, float c1, float c2)
{
    return clamp(1.0 / (1.0 + c1 * d + c1 * d * d), 0.0, 1.0);
}

float Attenuation_simple(float d)
{
    return clamp(1.0 / (d * d), 0.0, 1.0);
}

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
    // AddObj(dist, color, boxSDF(pos - vec2(0, 0), vec2(1, 1)), vec3(.6, .8, 1.));


    float thickness = 5.0;

    // Circle
    // float radius = 0.01;
    // float spacing = 0.15;
    // float angle = 0.0;
    // float angleIncrement = 0.1;
    // float radiusIncrement = 0.1;
    // float currentRadius = radius;

    // for (float i = 0.0; i < 10.0; i++) {
    //     vec2 center = vec2(0.0) + vec2(cos(angle), sin(angle)) * currentRadius;
    //     AddObj(dist, color, sphereSDF(pos - center, radius), vec3(1.0, 0.0, 0.0));
    //     angle += angleIncrement;
    //     currentRadius += radiusIncrement;
    // }

    vec2 center = vec2(0.0);
    // AddObj(dist, color, sphereSDF(pos - center, 0.1), vec3(0.0, 1.0, 0.0));
    // AddObj(dist, color, segmentSDF(pos, center + vec2(0.0, 0.0), center + vec2(0.0, -0.2)) - 0.02, vec3(0.0, 0.0, 1.0));
    // AddObj(dist, color, segmentSDF(pos, center + 0, center + vec2(0.2, -0.2)) - 0.02, vec3(1.0, 1.0, 0.0));

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

    // AddObj(dist, color, sphereSDF(
    //     pos - vec2(-5.0, 5.0), 
    //     1.), 
    //     vec3(100.0, 100.0, 100.0)
    // );


    // Random walls
    // float wallThickness = 0.01;
    // float wallLength = 0.2;
    // float numWalls = 10.0;
    // for (float i = 0.0; i < numWalls; i++) {
    //     vec2 start = vec2(hash(vec2(i, 0.0)) * 2.0 - 1.0, hash(vec2(i, 1.0)) * 2.0 - 1.0);
    //     vec2 end = start + vec2(hash(vec2(i, 2.0)) * wallLength, hash(vec2(i, 3.0)) * wallLength);
    //     AddObj(dist, color, segmentSDF(pos, start, end) - wallThickness, vec3(1., 1., 1.));
    // }
    // AddObj(dist, color, sphereSDF(pos - vec2(0, 0), 0.5), vec3(1, .9, .8));
    // AddObj(dist, color, sphereSDF(pos - vec2(.3 * sin(time), -2), 0.5), vec3(0, .1, 0));
    // AddObj(dist, color, boxSDF(pos - vec2(0, 1), vec2(1.5, 0.1)), vec3(.3, .1, .1));
    // for (int i = 0; i < 10; i++) {
    //     vec2 randomPos = vec2(hash(vec2(float(i), time)), hash(vec2(float(i + 10), time))) * 2.0 - 1.0;
    //     float randomSize = hash(vec2(float(i + 20), time)) * 0.5 + 0.1;
    //     vec3 randomColor = vec3(hash(vec2(float(i + 30), time)), hash(vec2(float(i + 40), time)), hash(vec2(float(i + 50), time)));
        // AddObj(dist, color, sphereSDF(pos - randomPos, randomSize), randomColor);
    // }
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

bool RayMarch(inout vec2 pos, in vec2 dir, out float td, out vec3 color)
{    
    td = 0.;
    for (int i = 0; i < MAX_STEPS; i++)
    {
        float d = 0.;
        Scene(pos, color, d);

        pos += dir * d;
                
        if (OutOfBounds(pos))
            break;

        if (d < EPSILON)
        {
            return true;
        }
        td += d;

    }
    return false;
}

void GenInitialRay(in vec2 uv, out vec2 pos, out vec2 dir)
{
    vec2 o;
    o = vec2(hash01(uv + time), hash01(uv - time)) * EPSILON;
    o = vec2(0.f);
    pos = uv + o;

    vec2 seed = pos + hash(vec2(float(samplesCurr), time));
    dir = RandomUnitCircleDir(seed);
}

void GenBounceRay(inout vec2 pos, inout vec2 dir)
{
    vec2 normal = GetSceneNormal(pos);
    pos += normal * EPSILON;
    
    vec2 seed = pos + hash(vec2(float(samplesCurr), time));
    dir = RandomUnitHfCircleDir(normal, seed);
}

void main()
{
    ivec2 st = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = (vec2(st) + 0.5) / vec2(resolution);
    uv = uv * 2.0 - 1.0;
    uv.x *= float(resolution.x) / float(resolution.y);

    vec2 cameraPos = vec2(0.0, 0.0);
    float cameraZoom = .1;

    uv = (uv - cameraPos) / cameraZoom;
    
    vec2 pos = vec2(0.);
    vec2 dir = vec2(0.);
    GenInitialRay(uv, pos, dir);

    float totalDist = 0.;
    vec3 contribution = vec3(0.);

    for (int b = 0; b < MAX_BOUNCE; b++)
    {
        vec3 col = vec3(0);
        bool hit = RayMarch(pos, dir, totalDist, col);

        if (!hit) 
            break;

        contribution += col * Attenuation(totalDist, 1.0, 1.0);
        GenBounceRay(pos, dir);
    }

    
    if (samplesCurr > 0)
    {
        contribution = (vec3(contribution) + (sceneGiBufferB[getIdx(st)] * float(samplesCurr - 1))) / float(samplesCurr);
    }
    float d;
    vec3 color;
    Scene(uv, color, d);

    sceneGiBufferA[getIdx(st)] = contribution;
    // sceneGiBufferA[getIdx(st)] = vec3(GetSceneNormal(uv), 0.);
    atomicAdd(rayCountBuffer, 1);
    // sceneGiBufferA[getIdx(uv)] = vec3(sceneNormalsBuffer[getIdx(uv)], 0.);
    // sceneGiBufferA[getIdx(uv)] = vec3(1.0);
}
