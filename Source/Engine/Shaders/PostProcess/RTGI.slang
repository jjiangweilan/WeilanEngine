import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Utils.Noise;
import Library.RayTracing;
import Lighting;

#define RTGI_RAW_MIP 0

// https://www.shadertoy.com/view/wslyWs
float3 Sky(float3 ro, float3 rd, float3 lightDir, bool hasSun)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y; 
    float2 p = (ro + dist * rd).xz;
    p *= 1.2 / SC;

    float3 skyCol = float3(0.3,0.5,0.85) - rd.y*rd.y*0.5;
    skyCol = lerp( skyCol, 0.85 * float3(0.7,0.75,0.85), pow( 1.0 - max(rd.y, 0.0), 4.0 ) );

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    // sun
    if (hasSun)
    {
        float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun = 0.25 * float3(1.0,0.7,0.4) * pow( sundot,5.0 );
        sun += 0.25 * float3(1.0,0.8,0.6) * pow( sundot,64.0 );
        sun += 0.2 * float3(1.0,0.8,0.6) * pow( sundot,512.0 );
        skyCol += sun;
    }

    // horizon
    skyCol = lerp( skyCol, 0.68 * float3(.418, .394, .372), pow( 1.0 - max(rd.y, 0.0), 16.0 ) );

    return skyCol * 0.45;
}

struct RTGIParameter
{
    float4 rtSize;
    float ray_multiplier;
    float bounce_count;
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

struct SurfaceHit
{
    bool hit;
    float3 position;
    float3 normal;
    float3 albedo;
    float3 emissive;
    float3 directLight;
};

SurfaceHit GetRayHitSurface(float3 rayOrigin, float3 rayDir)
{
    SurfaceHit result;
    result.hit = false;
    result.position = 0;
    result.normal = 0;
    result.albedo = 0;
    result.emissive = 0;
    result.directLight = 0;

    RayDesc ray;
    ray.Origin = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin = 0;
    ray.TMax = 1000.0;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(perMaterial.sceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, ray);

    while(q.Proceed());

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        result.hit = true;
        uint instanceID = q.CommittedInstanceID();
        uint geometryIndex = q.CommittedGeometryIndex();
        uint primitiveIndex = q.CommittedPrimitiveIndex();
        float2 bary2 = q.CommittedTriangleBarycentrics();

        ObjectTriangle tri = ObjectTriangle(perScene, perScene.rtObjectOffsets[instanceID], geometryIndex, primitiveIndex);
        GpuMaterial matData = tri.GetMaterial(perScene);

        RayHitVertexData hitData = tri.GetRayHit(perScene, bary2).TransformToWorld(tri.GetModelMatrix(), (float3x3)tri.GetInvModelMatrix());

        result.position = hitData.position;
        result.normal = hitData.normal;

        float4 albedo = matData.baseColorFactor;
        if (matData.baseColorTexIndex.x != 0xFFFFFFFF)
        {
            Texture2D tex = perScene.globalTextures[matData.baseColorTexIndex.x];
            albedo *= tex.SampleLevel(perScene.GetGlobalSampler(matData.baseColorTexIndex.y), hitData.uv, 0);
        }
        result.albedo = albedo.rgb;

        float3 emissive = matData.emissive.rgb;
        if (matData.emissiveMapTexIndex.x != 0xFFFFFFFF)
        {
            Texture2D tex = perScene.globalTextures[matData.emissiveMapTexIndex.x];
            emissive *= tex.SampleLevel(perScene.GetGlobalSampler(matData.emissiveMapTexIndex.y), hitData.uv, 0).rgb;
        }
        result.emissive = emissive;

        // Direct light evaluation at hit point
        Light mainLight = perScene.GetMainLight();
        if (mainLight.lightColor.a > 0.0) // If there is a light
        {
            float3 lightDir = normalize(mainLight.position.xyz);
            float attenuation = 1.0;
            if (mainLight.position.w != 0.0) // Point Light
            {
                float3 toLight = mainLight.position.xyz - hitData.position;
                float distSq = dot(toLight, toLight);
                lightDir = normalize(toLight);
                attenuation = 1.0 / max(1.0, distSq);
            }

            float ndotl = saturate(dot(hitData.normal, lightDir));
            if (ndotl > 0.0)
            {
                float3 shadowOrigin = OffsetRay(hitData.position, hitData.normal);
                bool inShadow = TraceVisibilityRay(perMaterial.sceneBVH, shadowOrigin, hitData.normal, lightDir, 0, 1000.0);
                if (!inShadow)
                {
                    result.directLight = mainLight.lightColor.rgb * mainLight.intensity * ndotl * attenuation;
                }
            }
        }
    }
    return result;
}

float3 IndirectRayHitLight(float3 rayOrigin, float3 rayDir)
{
    SurfaceHit hit = GetRayHitSurface(rayOrigin, rayDir);
    if (hit.hit)
    {
        return hit.emissive + hit.albedo * hit.directLight;
    }
    else
    {
        // Sky radiance
        return Sky(rayOrigin, rayDir, perScene.GetMainLight().position.xyz, perScene.GetMainLight().lightColor.a > 0.0) * 20;
    }
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
    float3 primaryAlbedo = perMaterial.albedoTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).rgb;

    float3 indirectLight = 0;
    bool full_rays = true;

    if (full_rays)
    {
        const uint totalBounce = uint(clamp(perMaterial.bounce_count, 1.0, 2.0));
        uint rayCount = 1 << (uint(clamp(perMaterial.ray_multiplier, 1.0, 11)) - 1);

        for(uint i = 0; i < rayCount; i++)
        {
            float3 currentThroughput = 1.0;
            float3 currentOrigin = posWS;
            float3 currentNormal = normalWS;
            float3 rayRadiance = 0;

            for(uint bounce = 0; bounce < totalBounce; bounce++)
            {
                // Use golden ratio sequence for arbitrary ray counts, offsetting by bounce
                float2 u = frac(float2(0.5, 0.5) + float(i * totalBounce + bounce) * float2(0.6180339887, 0.7548776662));
                float3 rayDir = GetCosineWeightedDirection(currentNormal, u);
                float3 rayOrigin = OffsetRay(currentOrigin, currentNormal);
                
                SurfaceHit hit = GetRayHitSurface(rayOrigin, rayDir);
                if (hit.hit)
                {
                    rayRadiance += currentThroughput * (hit.emissive + hit.albedo * hit.directLight);
                    currentThroughput *= hit.albedo;
                    currentOrigin = hit.position;
                    currentNormal = hit.normal;
                }
                else
                {
                    rayRadiance += currentThroughput * Sky(rayOrigin, rayDir, perScene.GetMainLight().position.xyz, perScene.GetMainLight().lightColor.a > 0.0) * 20;
                    break;
                }
            }
            indirectLight += rayRadiance;
        }

        indirectLight /= float(rayCount);
    }
    else
    {
        // Get noise — add golden-ratio temporal offset so each frame shoots a different ray
        // Decorrelate frames using the R2 low-discrepancy sequence (golden ratio + plastic constant).
        // Period = 8 frames — standard for 1-SPP real-time GI denoising.
        float2 randVal = perMaterial.noiseTex[(uint2(pixelCoord + hash_u32_v2(uint2(perScene.scene.frameIndex, perScene.scene.frameIndex + 1)))) % 128].xy;

        float3 rayDir = GetCosineWeightedDirection(normalWS, randVal);
        float3 rayOrigin = OffsetRay(posWS, normalWS);

        indirectLight = IndirectRayHitLight(rayOrigin, rayDir);
    }


    perMaterial.outRtgiTex[pixelCoord] = float4(indirectLight * primaryAlbedo, 1.0);
}
