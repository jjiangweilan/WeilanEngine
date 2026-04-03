import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Utils.Noise;
import Library.RayTracing;
import Lighting;

#define RTGI_RAW_MIP 0

struct RTGIParameter
{
    float4 rtSize;
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
    TextureCube environmentMap;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<RTGIParameter> perMaterial;

// Generate cosine weighted direction over hemisphere
float3 GetCosineWeightedDirection(float3 normal, float2 u)
{
    float phi = 2.0 * 3.14159265359 * u.x;
    float cosTheta = sqrt(u.y);
    float sinTheta = sqrt(1.0 - u.y);

    float3 tangent = normalize(cross(abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0), normal));
    float3 bitangent = cross(normal, tangent);

    return tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + normal * cosTheta;
}

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

    float2 uv = (float2(pixelCoord.xy) + 0.5f) * perMaterial.rtSize.zw;
    
    float depth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;
    if (depth <= 0.0000001)
    {
        perMaterial.outRtgiTex[pixelCoord] = float4(0, 0, 0, 1);
        return;
    }

    float4 posWS4 = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, depth, 1.0));
    float3 posWS = posWS4.xyz / posWS4.w;
    float3 normalWS = normalize(GetNormalWS(uv));

    // Get noise — add golden-ratio temporal offset so each frame shoots a different ray
    // Decorrelate frames using the R2 low-discrepancy sequence (golden ratio + plastic constant).
    // Period = 8 frames — standard for 1-SPP real-time GI denoising.
    float2 randVal = perMaterial.noiseTex[(uint2(pixelCoord + hash_u32_v2(uint2(perScene.scene.frameIndex, perScene.scene.frameIndex + 1)))) % 128].xy;

    float3 rayDir = GetCosineWeightedDirection(normalWS, randVal);
    float3 rayOrigin = OffsetRay(posWS, normalWS);

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin = 0.01;
    ray.TMax = 1000.0;

    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(perMaterial.sceneBVH, RAY_FLAG_NONE, 0xFF, ray);
    q.Proceed();

    float3 indirectLight = float3(0, 0, 0);

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        uint instanceID = q.CommittedInstanceID();
        uint geometryIndex = q.CommittedGeometryIndex();
        uint primitiveIndex = q.CommittedPrimitiveIndex();
        float2 bary2 = q.CommittedTriangleBarycentrics();

        RayHitVertexData hitData = GetHitVertexData(perScene, perScene.rtObjectOffsets, instanceID, geometryIndex, primitiveIndex, bary2);

        uint objectOffset = perScene.rtObjectOffsets[instanceID];
        GpuObject obj = perScene.GetGlobalBuffer().Load<GpuObject>(objectOffset);
        uint renderDataOffset = obj.pRenderDataOffset + geometryIndex * 16;
        GpuRenderData renderData = perScene.GetGlobalBuffer().Load<GpuRenderData>(renderDataOffset);
        GpuMaterial matData = perScene.GetGlobalBuffer().Load<GpuMaterial>(renderData.materialOffset);

        float4 albedo = matData.baseColorFactor;
        if (matData.baseColorTexIndex.x != 0xFFFFFFFF)
        {
            Texture2D tex = perScene.globalTextures[matData.baseColorTexIndex.x];
            albedo *= tex.SampleLevel(perScene.GetGlobalSampler(matData.baseColorTexIndex.y), hitData.uv, 0);
        }

        float3 emissive = matData.emissive.rgb;
        if (matData.emissiveMapTexIndex.x != 0xFFFFFFFF)
        {
            Texture2D tex = perScene.globalTextures[matData.emissiveMapTexIndex.x];
            emissive *= tex.SampleLevel(perScene.GetGlobalSampler(matData.emissiveMapTexIndex.y), hitData.uv, 0).rgb;
        }
        indirectLight += emissive;

        // Direct light evaluation at hit point
        Light mainLight = perScene.GetMainLight();
        if (mainLight.lightColor.a > 0.0) // If there is a light
        {
            float3 lightDir = normalize(mainLight.position.xyz);
            if (mainLight.position.w != 0.0)
            {
                lightDir = normalize(mainLight.position.xyz - hitData.position);
            }

            float ndotl = saturate(dot(hitData.normal, lightDir));
            if (ndotl > 0.0)
            {
                float3 shadowOrigin = OffsetRay(hitData.position, hitData.normal);
                bool inShadow = TraceVisibilityRay(perMaterial.sceneBVH, shadowOrigin, lightDir, 0.01, 10000.0);
                if (!inShadow)
                {
                    indirectLight += (albedo.rgb / 3.14159265359) * mainLight.lightColor.rgb * mainLight.intensity * ndotl;
                }
            }
        }
    }
    else
    {
        indirectLight = perMaterial.environmentMap.SampleLevel(perMaterial.sampler_linear_clamp, rayDir, 0).rgb;
    }

    perMaterial.outRtgiTex[pixelCoord] = float4(indirectLight, 1.0);
}
