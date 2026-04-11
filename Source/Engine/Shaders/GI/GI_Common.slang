/**
 * GI_Common.slang
 *
 * Shared utility functions for Global Illumination passes (GI and RTGI).
 * Follows the same pattern as Library/RayTracing.slang: GPU resource handles
 * (RaytracingAccelerationStructure, PerScene) are passed as explicit parameters
 * so this module can be imported by any shader without declaring its own
 * global parameter blocks.
 */

import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Library.RayTracing;
import Lighting;
import Utils.Noise;

// ---------------------------------------------------------------------------
// Hemisphere sampling
// ---------------------------------------------------------------------------

/// Generates a cosine-weighted direction over the hemisphere defined by normal.
/// u: two uniform random numbers in [0,1).
float3 GetCosineWeightedDirection(float3 normal, float2 u)
{
    float phi      = 2.0 * 3.14159265359 * u.x;
    float cosTheta = sqrt(u.y);
    float sinTheta = sqrt(1.0 - u.y);

    float3 tangent   = normalize(cross(abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0), normal));
    float3 bitangent = cross(normal, tangent);

    return tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + normal * cosTheta;
}

float3 GetSemiSphereDirection(float3 normal, float2 uv, out float sinTheta)
{
    float phi = 2.0 * 3.14159265359 * uv.x;
    float cosTheta = uv.y;
    sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 tangent   = normalize(cross(abs(normal.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0), normal));
    float3 bitangent = cross(normal, tangent);

    return tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + normal * cosTheta;
}

// ---------------------------------------------------------------------------
// Procedural sky
// ---------------------------------------------------------------------------

/// Returns approximate sky radiance for ray direction rd.
/// https://www.shadertoy.com/view/wslyWs
float3 Sky(float3 ro, float3 rd, float3 lightDir, bool hasSun)
{
    const float SC = 1e5;

    float dist = (SC - ro.y) / rd.y;
    float2 p   = (ro + dist * rd).xz;
    p         *= 1.2 / SC;

    float3 skyCol = float3(0.3, 0.5, 0.85) - rd.y * rd.y * 0.5;
    skyCol = lerp(skyCol, 0.85 * float3(0.7, 0.75, 0.85), pow(1.0 - max(rd.y, 0.0), 4.0));

    if (hasSun)
    {
        float sundot  = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun    = 0.25 * float3(1.0, 0.7, 0.4) * pow(sundot, 5.0);
        sun          += 0.25 * float3(1.0, 0.8, 0.6) * pow(sundot, 64.0);
        sun          += 0.2  * float3(1.0, 0.8, 0.6) * pow(sundot, 512.0);
        skyCol       += sun;
    }

    skyCol = lerp(skyCol, 0.68 * float3(0.418, 0.394, 0.372), pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol * 0.45;
}

// ---------------------------------------------------------------------------
// Ray-hit surface data
// ---------------------------------------------------------------------------

struct SurfaceHit
{
    bool   hit;
    float3 position;
    float3 normal;
    float3 albedo;
    float3 emissive;
    float3 directLight;
};

/// Traces a single ray and returns surface data at the first hit.
/// bvh:      scene TLAS.
/// scene:    PerScene parameter block (read-only; passed explicitly so this
///           module avoids declaring its own global parameter block).
SurfaceHit GetRayHitSurface(
    RaytracingAccelerationStructure bvh,
    ParameterBlock<PerScene> scene,
    float3 rayOrigin,
    float3 rayDir)
{
    SurfaceHit result;
    result.hit         = false;
    result.position    = 0;
    result.normal      = 0;
    result.albedo      = 0;
    result.emissive    = 0;
    result.directLight = 0;

    RayDesc ray;
    ray.Origin    = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin      = 0;
    ray.TMax      = 1000.0;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(bvh, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, ray);

    while (q.Proceed());

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        result.hit = true;

        uint instanceID    = q.CommittedInstanceID();
        uint geometryIndex = q.CommittedGeometryIndex();
        uint primitiveIndex = q.CommittedPrimitiveIndex();
        float2 bary2       = q.CommittedTriangleBarycentrics();

        ObjectTriangle tri     = ObjectTriangle(scene, scene.rtObjectOffsets[instanceID], geometryIndex, primitiveIndex);
        GpuMaterial    matData = tri.GetMaterial(scene);

        RayHitVertexData hitData = tri.GetRayHit(scene, bary2)
            .TransformToWorld(tri.GetModelMatrix(), (float3x3)tri.GetInvModelMatrix());

        result.position = hitData.position;
        result.normal   = hitData.normal;

        float4 albedo = matData.baseColorFactor;
        if (matData.baseColorTexIndex.x != 0xFFFFFFFF)
        {
            Texture2D tex = scene.globalTextures[matData.baseColorTexIndex.x];
            albedo *= tex.SampleLevel(scene.GetGlobalSampler(matData.baseColorTexIndex.y), hitData.uv, 0);
        }
        result.albedo = albedo.rgb;

        float3 emissive = matData.emissive.rgb;
        if (matData.emissiveMapTexIndex.x != 0xFFFFFFFF)
        {
            Texture2D tex = scene.globalTextures[matData.emissiveMapTexIndex.x];
            emissive *= tex.SampleLevel(scene.GetGlobalSampler(matData.emissiveMapTexIndex.y), hitData.uv, 0).rgb;
        }
        result.emissive = emissive;

        Light mainLight = scene.GetMainLight();
        if (mainLight.lightColor.a > 0.0)
        {
            float3 lightDir  = normalize(mainLight.position.xyz);
            float attenuation = 1.0;
            if (mainLight.position.w != 0.0)
            {
                float3 toLight = mainLight.position.xyz - hitData.position;
                float  distSq  = dot(toLight, toLight);
                lightDir       = normalize(toLight);
                attenuation    = 1.0 / max(1.0, distSq);
            }

            float ndotl = saturate(dot(hitData.normal, lightDir));
            if (ndotl > 0.0)
            {
                float3 shadowOrigin = OffsetRay(hitData.position, hitData.normal);
                bool inShadow = TraceVisibilityRay(bvh, shadowOrigin, hitData.normal, lightDir, 0, 1000.0);
                if (!inShadow)
                {
                    result.directLight = mainLight.lightColor.rgb * mainLight.intensity * ndotl * attenuation;
                }
            }
        }
    }

    return result;
}

/// Traces a single ray and returns the incoming radiance:
/// hit  → emissive + albedo * directLight
/// miss → procedural sky
float3 IndirectRayHitLight(
    RaytracingAccelerationStructure bvh,
    ParameterBlock<PerScene> scene,
    float3 rayOrigin,
    float3 rayDir)
{
    SurfaceHit hit = GetRayHitSurface(bvh, scene, rayOrigin, rayDir);
    if (hit.hit)
    {
        return hit.emissive + hit.albedo * hit.directLight;
    }
    else
    {
        Light mainLight = scene.GetMainLight();
        return Sky(rayOrigin, rayDir, mainLight.position.xyz, mainLight.lightColor.a > 0.0) * 20;
    }
}

int2 _GetZOrderCoord2x2(uint index)
{
    // index is expected to be between 0 and 3
    uint x = index & 0x01;
    uint y = (index & 0x02) >> 1;
    
    return int2(x, y);
}

uint2 GetTileOffset(uint2 coord, uint frameIndex, uint2 shProbeRtSize)
{
    return hash_u32_v2(uint2(frameIndex, coord.x + coord.y * shProbeRtSize.x)) % 2;
}

uint2 GetSHProbeResolution(float2 fullResRtSize)
{
    uint2 size = uint2(uint(fullResRtSize.x + 1) / 2, uint(fullResRtSize.y + 1) / 2);
    return size;
}

float2 GetFullResUv(uint2 fullResCoord, float4 fullResRtSize)
{
    float2 fullResUv = (float2(min(fullResCoord, fullResRtSize.xy - 1)) + 0.5f) * fullResRtSize.zw;
    return fullResUv;
}

float2 GI_FromFullResUv_to_SHProbeUv(float2 fullResUv, float2 fullResRtSize, float2 shProbeTexelSize)
{
    float2 fullResPixelCoord = fullResUv * fullResRtSize;
    return uint2(fullResPixelCoord / 2) * shProbeTexelSize;
}
