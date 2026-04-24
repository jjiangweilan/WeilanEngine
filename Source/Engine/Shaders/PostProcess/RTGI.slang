import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Utils.Noise;
import Library.RayTracing;
import Lighting;
import GI.GI_Common;

#define RTGI_RAW_MIP 0

struct RTGIParameter
{
    float4 rtSize;
    float ray_multiplier;
    float secondary_bounce;
    Texture2D depthTex;
    Texture2D normalTex;
    Texture2D albedoTex;
    Texture2D<float4> noiseTex;
    RWTexture2D<float4> outRtgiTex;
    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_repeat;
    SamplerState sampler_linear_repeat;

    RaytracingAccelerationStructure sceneBVH;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<RTGIParameter> perMaterial;

float3 GetNormalWS(float2 uv)
{
    float3 normalWS = perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xyz;
    return DecodeGBufferNormal(normalWS);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    if (pixelCoord.x >= perMaterial.rtSize.x || pixelCoord.y >= perMaterial.rtSize.y)
        return;

    float2 uv    = (float2(pixelCoord.xy) + 0.5f) * perMaterial.rtSize.zw;
    float  depth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;
    if (depth <= 0.0000001)
    {
        perMaterial.outRtgiTex[pixelCoord] = float4(0, 0, 0, 1);
        return;
    }

    float4 posWS4        = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, depth, 1.0));
    float3 posWS         = posWS4.xyz / posWS4.w;
    float3 normalWS      = normalize(GetNormalWS(uv));
    float3 primaryAlbedo = perMaterial.albedoTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).rgb;

    const uint totalBounce = perMaterial.secondary_bounce > 0.5 ? 2 : 1;
    uint rayCount = 1 << (uint(clamp(perMaterial.ray_multiplier, 1.0, 11)) - 1);

    float3 indirectLight = 0;
    for (uint i = 0; i < rayCount; i++)
    {
        float3 currentThroughput = 1.0;
        float3 currentOrigin     = posWS;
        float3 currentNormal     = normalWS;
        float3 rayRadiance       = 0;

        for (uint bounce = 0; bounce < totalBounce; bounce++)
        {
            // Golden ratio sequence for decorrelated multi-bounce sampling
            float2 u = frac(float2(0.5, 0.5) + float(i * totalBounce + bounce) * float2(0.6180339887, 0.7548776662));
            float3 rayDir    = GetCosineWeightedDirection(currentNormal, u);
            float3 rayOrigin = OffsetRay(currentOrigin, currentNormal);

            SurfaceHit hit = GetRayHitSurface(perMaterial.sceneBVH, perScene, rayOrigin, rayDir);
            if (hit.hit)
            {
                rayRadiance      += currentThroughput * (hit.emissive + hit.albedo * hit.directLight);
                currentThroughput *= hit.albedo;
                currentOrigin     = hit.position;
                currentNormal     = hit.normal;
            }
            else
            {
                Light mainLight  = perScene.GetMainLight();
                rayRadiance     += currentThroughput * Sky(rayOrigin, rayDir, mainLight.position.xyz, mainLight.lightColor.a > 0.0) * 20;
                break;
            }
        }
        indirectLight += rayRadiance;
    }

    indirectLight /= float(rayCount);

    perMaterial.outRtgiTex[pixelCoord] = float4(indirectLight * primaryAlbedo, 1.0);
}
