#version 430

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 1) uniform sampler2D jfaTex_A;
layout(binding = 2, rgba32f) uniform  image2D jfaImg_B;

uniform ivec2 resolution;
uniform int stepWidth;

#define INF 1E9
#define getIdx(uv) ((uv).x + resolution.x * (uv).y)

void JumpFlood(inout vec4 jfaOut, in vec2 uv, in vec2 dir)
{
    uv += dir * float(stepWidth);
    vec3 samplePos = texture(jfaTex_A, uv).xyz;

	if(samplePos.z < 1.0)
		return;

	if(samplePos.x == 0.0 && samplePos.y == 0.0 )
		return;

    float dist = length(jfaOut.xy - samplePos.xy);

    if(dist < jfaOut.w)
    {
        jfaOut = vec4(samplePos.xy, 1.0, dist);
    }
}

void main() 
{
    ivec2 st = ivec2(gl_GlobalInvocationID.xy);
    vec2 texelSize = 1. / vec2(resolution);

	vec2 uv = st * texelSize;
	vec4 initialSeed = texture(jfaTex_A, st * texelSize); 
    
	vec4 jfaOut = initialSeed;
    jfaOut.w = INF;

    for(int x = -1; x <= 1; x++)
    {
        for(int y = -1; y <= 1; y++)
        {
            vec2 dir = vec2(x, y);
			vec2 uv_offset = st * texelSize + dir * float(stepWidth) * texelSize;
			vec3 samplePos = texture(jfaTex_A, uv_offset).xyz;

			float dist = length(uv - samplePos.xy);

			if( samplePos.z == 1.0 && (dist < jfaOut.w || jfaOut.z != 1.0))
			{
				jfaOut.xy = samplePos.xy;
				jfaOut.z = 1.0;
				jfaOut.w = dist;
			}
        }
    }

    imageStore(jfaImg_B, st, jfaOut);
}