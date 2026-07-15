import WeilanEngine;
import Library.PerScene;
#include "Utils/VertexFullScreen.hlsl"

#if CONFIG
blend:
    - one srcAlpha zero one
depth:
    testEnable: False
    writeEnable: False
#endif

struct DepthBasedFogParams
{
    float4 fogColor;
    float  fogDensity;

    Texture2D depthTexture;
    SamplerState LinearClampSampler;
};

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<DepthBasedFogParams> shaderInput;

[shader("fragment")]
float4 FragmentShader(VertexOutput input) : SV_Target
{
    Texture2D depthTexture = shaderInput.depthTexture;

    float2 uv = input.uv;
    float depth = depthTexture.Sample(shaderInput.LinearClampSampler, uv).r;
    float3 worldPos = perScene.NDCToWorld(float3(uv * 2 - 1, depth));

    float distance = perScene.DistanceToCamera(worldPos);

    // float fogFactor = saturate((distance - fogStart) / (fogEnd - fogStart)); // linear
    float fogIntensity = min(0.8, saturate(1.0 - exp(-distance * shaderInput.fogDensity))); // exponential

    return float4(shaderInput.fogColor.xyz * fogIntensity, 1 - fogIntensity);
}
