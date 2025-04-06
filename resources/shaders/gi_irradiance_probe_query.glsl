#version 430

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D probeIrradiancDepthImage;

layout(std430, binding = 2) writeonly buffer sceneGiLayout
{
    vec3 sceneGiBuffer[];
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

const int DEFAULT_PROBE_WIDTH = 64;
const int DEFAULT_PROBE_HEIGHT = 64;
const int DEFAULT_PROBE_ANGLE_RESOLUTION = 16;
const int DEFAULT_PROBE_LOCAL_SIZE = int(sqrt(float(DEFAULT_PROBE_ANGLE_RESOLUTION)));

// https://learnwebgl.brown37.net/09_lights/lights_attenuation.html
float Attenuation(float d, float c1, float c2)
{
    return clamp(1.0 / (1.0 + c1 * d + c1 * d * d), 0.0, 1.0);
}

float Attenuation_simple(float d)
{
    return clamp(1.0 / (d * d), 0.0, 1.0);
}

vec2 getProbeUV(ivec2 probeIdx, vec2 dir, vec2 probeTextureSize) {
    // Normalize the direction and compute the angle in [0, 2π]
    vec2 normDir = normalize(dir);
    float theta = atan(normDir.y, normDir.x);
    if (theta < 0.0)
        theta += 2.0 * PI;
    
    // Compute the fractional angular index
    float angleStep = 2.0 * PI / float(DEFAULT_PROBE_ANGLE_RESOLUTION);
    float angleIndex = theta / angleStep;
    
    // Determine the two closest bins
    int lowerBin = int(floor(angleIndex)) % DEFAULT_PROBE_ANGLE_RESOLUTION;
    int upperBin = (lowerBin + 1) % DEFAULT_PROBE_ANGLE_RESOLUTION;
    float weight = fract(angleIndex);
    
    // Convert the bins into 2D local indices
    ivec2 localIdx0 = ivec2(lowerBin % DEFAULT_PROBE_LOCAL_SIZE, lowerBin / DEFAULT_PROBE_LOCAL_SIZE);
    ivec2 localIdx1 = ivec2(upperBin % DEFAULT_PROBE_LOCAL_SIZE, upperBin / DEFAULT_PROBE_LOCAL_SIZE);
    
    // Convert the indices to probe texture UV coordinates
    vec2 uv0 = (vec2(probeIdx * DEFAULT_PROBE_LOCAL_SIZE + localIdx0) + 0.5) / probeTextureSize;
    vec2 uv1 = (vec2(probeIdx * DEFAULT_PROBE_LOCAL_SIZE + localIdx1) + 0.5) / probeTextureSize;
    
    // Interpolate between the two UVs using the fractional weight
    return mix(uv0, uv1, weight);
}

void main()
{
    ivec2 st = ivec2(gl_GlobalInvocationID.xy);
    vec2 uv = vec2(st) / vec2(uResolution);
    
    vec2 probePos = uv * vec2(DEFAULT_PROBE_WIDTH);
    ivec2 i0 = ivec2(floor(probePos));
    ivec2 i1 = clamp(i0 + ivec2(1, 0), ivec2(0), ivec2(DEFAULT_PROBE_WIDTH - 1));
    ivec2 i2 = clamp(i0 + ivec2(0, 1), ivec2(0), ivec2(DEFAULT_PROBE_WIDTH - 1));
    ivec2 i3 = clamp(i0 + ivec2(1, 1), ivec2(0), ivec2(DEFAULT_PROBE_WIDTH - 1));

    vec2 d0 = normalize(probePos - vec2(i0));
    vec2 d1 = normalize(probePos - vec2(i1));
    vec2 d2 = normalize(probePos - vec2(i2));
    vec2 d3 = normalize(probePos - vec2(i3));

    vec2 probeTextureSize = vec2(textureSize(probeIrradiancDepthImage, 0));

    vec2 uv0 = getProbeUV(i0, d0, probeTextureSize);
    vec2 uv1 = getProbeUV(i1, d1, probeTextureSize);
    vec2 uv2 = getProbeUV(i2, d2, probeTextureSize);
    vec2 uv3 = getProbeUV(i3, d3, probeTextureSize);

    vec3 c0 = texture(probeIrradiancDepthImage, uv0).rgb;
    vec3 c1 = texture(probeIrradiancDepthImage, uv1).rgb;
    vec3 c2 = texture(probeIrradiancDepthImage, uv2).rgb;
    vec3 c3 = texture(probeIrradiancDepthImage, uv3).rgb;

    vec2 f = fract(probePos);
    
    vec3 top = mix(c0, c1, f.x);
    vec3 bottom = mix(c2, c3, f.x);
    vec3 irradiance = mix(top, bottom, f.y);
    
    uint idx = getIdx(st);

    sceneGiBuffer[idx] = texture(probeIrradiancDepthImage, uv).rgb;
    // sceneGiBuffer[idx] = vec3(uv, 0.0);
}