import WeilanEngine;
import Library.PerScene;
#include "Particle.hlsl"

#if CONFIG
name : Particle
cull: none
depth:
    testEnable: true
    writeEnable: false
blend:
    - srcAlpha oneMinusSrcAlpha
#endif

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct ParticleParameter
{
    float4 config;
    float4 screenSizeRange;
    StructuredBuffer<GPUParticle> particles;
    Texture2D mainTexture;
    SamplerState sampler_linear_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<ParticleParameter> param;

float3 RotateByQuaternion(float3 value, float4 rotation)
{
    return value + 2.0 * cross(rotation.xyz, cross(rotation.xyz, value) + rotation.w * value);
}

[shader("vertex")]
VertexOutput Vert(VertexInput input, uint instanceID : SV_InstanceID)
{
    GPUParticle particle = param.particles[instanceID];
    VertexOutput output;
    float3 center = particle.positionAndFrame.xyz;
    float3 scale = particle.scaleAndMode.xyz;
    uint renderMode = uint(particle.scaleAndMode.w + 0.5);
    float3 worldPosition;

    if (renderMode == 0)
    {
        worldPosition = center + RotateByQuaternion(input.position * scale, particle.rotation);
    }
    else
    {
        float viewDepth = abs(mul(perScene.camera.view, float4(center, 1.0)).z);
        float projectedHeight = abs(scale.y * perScene.camera.projection[1][1]) / max(viewDepth, 0.0001);
        float minimumScreenSize = max(param.screenSizeRange.x, 0.0);
        float maximumScreenSize = max(param.screenSizeRange.y, minimumScreenSize);
        if (projectedHeight > 0.0001)
            scale *= clamp(projectedHeight, minimumScreenSize, maximumScreenSize) / projectedHeight;

        float2 quad = input.position.xz - param.config.zw;
        float angle = particle.sortData.z;
        float sine;
        float cosine;
        sincos(angle, sine, cosine);
        quad = float2(cosine * quad.x - sine * quad.y, sine * quad.x + cosine * quad.y);

        float3 cameraRight = normalize(perScene.camera.invView[0].xyz);
        float3 cameraUp = normalize(perScene.camera.invView[1].xyz);
        float3 viewDirection = normalize(perScene.camera.position.xyz - center);
        float3 right = cameraRight;
        float3 up = cameraUp;

        if (renderMode == 2)
        {
            up = float3(0.0, 1.0, 0.0);
            right = normalize(cross(up, viewDirection));
        }
        else if (renderMode == 3)
        {
            right = float3(1.0, 0.0, 0.0);
            up = float3(0.0, 0.0, 1.0);
        }
        else if (renderMode == 4)
        {
            float velocityLength = length(particle.velocityAndStretch.xyz);
            up = velocityLength > 0.0001 ? particle.velocityAndStretch.xyz / velocityLength : cameraUp;
            right = normalize(cross(up, viewDirection));
            scale.y *= particle.velocityAndStretch.w;
        }

        worldPosition = center + right * quad.x * scale.x + up * quad.y * scale.y;
    }

    uint columns = max(uint(param.config.x), 1u);
    uint rows = max(uint(param.config.y), 1u);
    uint frame = uint(particle.positionAndFrame.w) % (columns * rows);
    float2 tileSize = 1.0 / float2(columns, rows);
    float2 tile = float2(frame % columns, frame / columns);
    output.uv = (input.uv + tile) * tileSize;
    output.position = perScene.WorldToClipSpace(worldPosition);
    output.color = particle.color;
    return output;
}

[shader("fragment")]
float4 Frag(VertexOutput input) : SV_Target
{
    return param.mainTexture.Sample(param.sampler_linear_clamp, input.uv) * input.color;
}
