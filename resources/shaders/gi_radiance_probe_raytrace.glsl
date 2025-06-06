#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D colorMaskTex;
layout(binding = 2) uniform sampler2D sdfTex;
layout(binding = 3) uniform sampler2D normalsTex;

layout(binding = 4, rgba32f) uniform image2D probeIrradiancDepthImage;

layout(std430, binding = 5) buffer rayCountLayout
{
    uint rayCountBuffer;
};

uniform ivec2 uResolution;
uniform uint uSamples;
uniform float uTime;

#define INF 1E4
#define PI 3.14159265359
#define EPSILON 1E-9
#define MAX_STEPS 100
#define MAX_BOUNCE 8

#define getIdx(uv) (uv.x)+uResolution.x*(uv.y)

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
    vec2 bound = vec2(float(uResolution.x) / float(uResolution.y), 1.1);
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

void Scene(in vec2 pos, out vec3 color, out float d)
{
    vec4 colorMask = texture(colorMaskTex, pos);
    color = colorMask.rgb * colorMask.a;
    d = texture(sdfTex, pos).r;
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
    o = vec2(hash01(uv + uTime), hash01(uv - uTime)) * EPSILON;
    o = vec2(0.f);
    pos = uv + o;

    vec2 seed = pos + hash(vec2(float(uSamples), uTime));
    dir = RandomUnitCircleDir(seed);
}

void GenBounceRay(inout vec2 pos, inout vec2 dir)
{
    vec2 normal = texture(normalsTex, pos).xy;
    pos += normal * 1E-4;
    
    vec2 seed = pos + hash(vec2(float(uSamples), uTime));
    dir = RandomUnitHfCircleDir(normal, seed);
}

const int DEFAULT_PROBE_WIDTH = 64;
const int DEFAULT_PROBE_HEIGHT = 64;
const int DEFAULT_PROBE_ANGLE_RESOLUTION = 16;
const int DEFAULT_PROBE_LOCAL_SIZE = int(sqrt(float(DEFAULT_PROBE_ANGLE_RESOLUTION)));

ivec2 getProbeIdx(ivec2 probeIdx, vec2 dir)
{
    vec2 normDir = normalize(dir);

    float theta = atan(normDir.y, normDir.x);
    if (theta < 0.0)
        theta += 2.0 * PI; // Ensure theta is in the range [0, 2*PI]

    float angleStep = 2.0 * PI / float(DEFAULT_PROBE_ANGLE_RESOLUTION);

    int localIdxX = int(theta / angleStep) % DEFAULT_PROBE_LOCAL_SIZE;
    int localIdxY = int((theta / angleStep) / float(DEFAULT_PROBE_LOCAL_SIZE));

    ivec2 localIdx = ivec2(localIdxX, localIdxY);

    ivec2 globalIdx = probeIdx * DEFAULT_PROBE_LOCAL_SIZE + localIdx;

    return globalIdx;
}

vec2 SubpixelJitter(vec2 uv, float sampleIndex)
{
    float jitterX = hash01(vec2(sampleIndex, uv.x)) * 2.0 - 1.0;
    float jitterY = hash01(vec2(sampleIndex, uv.y)) *2.0 - 1.0;
    return uv + vec2(jitterX, jitterY) / vec2(uResolution);
}

void main()
{

    ivec2 st = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = (vec2(st) + 0.5) / vec2(imageSize(probeIrradiancDepthImage).xy);
    
    vec2 pos = vec2(0.);
    vec2 dir = vec2(0.);
    uv = SubpixelJitter(uv, uSamples);
    GenInitialRay(uv, pos, dir);

    if(texture(sdfTex, pos).r <= 0)
    {
        return;
    }

    vec2 dir_0 = dir;

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

    // ivec2 idx = getProbeIdx(st, dir_0);
    ivec2 idx = ivec2(st);
    vec4 prevValue = imageLoad(probeIrradiancDepthImage, idx);
    vec3 accumulatedValue = (prevValue.rgb * float(uSamples - 1) + contribution) / float(uSamples + 1);
    imageStore(probeIrradiancDepthImage, idx, vec4(accumulatedValue, 0.));

    // imageStore(probeIrradiancDepthImage, idx, vec4(contribution, 0.0)); 

    // if (uSamples > 0)
    // {
    //     contribution = (vec3(contribution) + (sceneGiBufferB[getIdx(st)] * float(uSamples - 1))) / float(uSamples);
    // }

    // sceneGiBufferA[getIdx(st)] = contribution;
    // atomicAdd(rayCountBuffer, 1);

    // sceneGiBufferA[getIdx(st)] = vec3(GetSceneNormal(uv), 0.);
    // sceneGiBufferA[getIdx(uv)] = vec3(sceneNormalsBuffer[getIdx(uv)], 0.);
    // sceneGiBufferA[getIdx(uv)] = vec3(1.0);
}
