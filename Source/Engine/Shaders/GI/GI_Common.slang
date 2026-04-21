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
    float  rayDistance;
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
    float3 rayDir, uint2 debug = uint2(0,0))
{
    SurfaceHit result;
    result.hit         = false;
    result.rayDistance = -1.0;
    result.position    = 0;
    result.normal      = 0;
    result.albedo      = 0;
    result.emissive    = 0;
    result.directLight = 0;

    RayDesc ray;
    ray.Origin    = rayOrigin;
    ray.Direction = rayDir;
    ray.TMin      = 0;
    ray.TMax      = 30.0;

    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(bvh, RAY_FLAG_NONE, 0xFF, ray);

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
        result.rayDistance = distance(hitData.position, rayOrigin);

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

float3 DecodeGIWorldNormal(Texture2D normalTex, SamplerState pointSampler, float2 uv)
{
    float3 normalWS = normalTex.SampleLevel(pointSampler, uv, 0).xyz;
    return DecodeGBufferNormal(normalWS);
}

struct SparseRaySample
{
    bool valid;
    float3 radiance;
    float3 rayDir;
    float3 sourceNormal;
    float3 sourcePosition;
    float2 sourceUv;
};

SparseRaySample MakeInvalidSparseRaySample()
{
    SparseRaySample sample;
    sample.valid = false;
    sample.radiance = 0;
    sample.rayDir = 0;
    sample.sourceNormal = 0;
    sample.sourcePosition = 0;
    sample.sourceUv = 0;
    return sample;
}

bool LoadProbeGeometry(
    ParameterBlock<PerScene> scene,
    Texture2D hierarchyDepth,
    Texture2D normalTex,
    SamplerState pointSampler,
    float4 rtSize,
    float distanceScale,
    uint2 probeCoord,
    out uint2 probeFullResCoord,
    out float2 probeUv,
    out float3 probePosition,
    out float3 probeNormal,
    out float probePlaneThreshold)
{
    probeFullResCoord = GetSHProbeSourceCoord(probeCoord, rtSize.xy);
    probeUv = GetFullResUv(probeFullResCoord, rtSize);

    float depth = hierarchyDepth.SampleLevel(pointSampler, probeUv, 0).x;
    if (depth <= 0.0000001)
    {
        probePosition = 0;
        probeNormal = 0;
        probePlaneThreshold = 0;
        return false;
    }

    probePosition = ReconstructWorldPos(scene, probeUv, depth);
    probeNormal = normalize(DecodeGIWorldNormal(normalTex, pointSampler, probeUv));
    probePlaneThreshold = GetAdaptivePlaneDistanceThreshold(scene, distanceScale, probePosition);
    return true;
}

uint2 GetSHProbeResolution(float2 fullResRtSize)
{
    uint2 size = uint2(uint(fullResRtSize.x + 1) / 2, uint(fullResRtSize.y + 1) / 2);
    return size;
}

uint2 GetRayAtlasResolution(float2 fullResRtSize)
{
    return uint2(uint(fullResRtSize.x + 3) / 4, uint(fullResRtSize.y + 3) / 4);
}

uint2 ClampFullResCoord(uint2 coord, float2 fullResRtSize)
{
    uint2 maxCoord = uint2(fullResRtSize) - 1;
    return min(coord, maxCoord);
}

uint2 GetRayTileOffset(uint2 coord, uint frameIndex, uint2 rayRtSize)
{
    uint linearIndex = coord.x + coord.y * max(rayRtSize.x, 1);
    return hash_u32_v2(uint2(frameIndex, linearIndex)) % 4;
}

uint2 GetRaySourceCoord(uint2 rayCoord, uint frameIndex, float2 fullResRtSize)
{
    uint2 rayRtSize = GetRayAtlasResolution(fullResRtSize);
    uint2 tileOffset = GetRayTileOffset(rayCoord, frameIndex, rayRtSize);
    return ClampFullResCoord(rayCoord * 4 + tileOffset, fullResRtSize);
}

uint2 GetSHProbeSourceCoord(uint2 probeCoord, float2 fullResRtSize)
{
    return ClampFullResCoord(probeCoord * 2, fullResRtSize);
}

uint2 GetSHProbeCoordFromFullResCoord(uint2 fullResCoord, float2 fullResRtSize)
{
    uint2 probeRtSize = GetSHProbeResolution(fullResRtSize);
    return min(fullResCoord / 2, probeRtSize - 1);
}

float GetSHProbeGridFromFullResCoord1D(float fullResCoord, uint fullResSize)
{
    float clampedFullResCoord = clamp(fullResCoord, 0.0, float(fullResSize - 1));
    uint probeSize = (fullResSize + 1) / 2;
    uint probeCoord = min((uint)(clampedFullResCoord * 0.5), probeSize - 1);
    uint nextProbeCoord = min(probeCoord + 1, probeSize - 1);
    float probeSourceCoord = float(min(probeCoord * 2, fullResSize - 1));
    float nextProbeSourceCoord = float(min(nextProbeCoord * 2, fullResSize - 1));
    if (nextProbeSourceCoord <= probeSourceCoord)
        return (float)probeCoord;

    // Interpolate between the representative full-res probe anchors so odd
    // render-target sizes keep the last partial tile aligned to the last pixel.
    return (float)probeCoord + ((clampedFullResCoord - probeSourceCoord) / (nextProbeSourceCoord - probeSourceCoord));
}

float2 GetSHProbeGridFromFullResCoord(uint2 fullResCoord, float2 fullResRtSize)
{
    uint2 fullResSize = uint2(fullResRtSize);
    return float2(
        GetSHProbeGridFromFullResCoord1D((float)fullResCoord.x, fullResSize.x),
        GetSHProbeGridFromFullResCoord1D((float)fullResCoord.y, fullResSize.y)
    );
}

float2 GetSHProbeGridFromFullResUv(float2 fullResUv, float2 fullResRtSize)
{
    uint2 fullResSize = uint2(fullResRtSize);
    return float2(
        GetSHProbeGridFromFullResCoord1D(fullResUv.x * fullResRtSize.x - 0.5, fullResSize.x),
        GetSHProbeGridFromFullResCoord1D(fullResUv.y * fullResRtSize.y - 0.5, fullResSize.y)
    );
}

uint2 GetSHProbeCoordFromFullResUv(float2 fullResUv, float2 fullResRtSize)
{
    uint2 probeRtSize = GetSHProbeResolution(fullResRtSize);
    float2 probeGrid = GetSHProbeGridFromFullResUv(fullResUv, fullResRtSize);
    return min(uint2(probeGrid + 0.5), probeRtSize - 1);
}

float2 GetFullResUv(uint2 fullResCoord, float4 fullResRtSize)
{
    float2 fullResUv = (float2(min(fullResCoord, fullResRtSize.xy - 1)) + 0.5f) * fullResRtSize.zw;
    return fullResUv;
}

SparseRaySample LoadSparseRaySample(
    ParameterBlock<PerScene> scene,
    Texture2D hierarchyDepth,
    Texture2D normalTex,
    SamplerState pointSampler,
    Texture2D<float4> rayDataTex,
    Texture2D<float4> rayMetaTex,
    float4 rtSize,
    uint2 rayCoord,
    uint frameIndex)
{
    SparseRaySample sample = MakeInvalidSparseRaySample();

    uint2 sourceCoord = GetRaySourceCoord(rayCoord, frameIndex, rtSize.xy);
    float2 sourceUv = GetFullResUv(sourceCoord, rtSize);
    float sourceDepth = hierarchyDepth.SampleLevel(pointSampler, sourceUv, 0).x;
    if (sourceDepth <= 0.0000001)
        return sample;

    float4 rayData = rayDataTex.Load(int3(rayCoord, 0));
    float4 rayMeta = rayMetaTex.Load(int3(rayCoord, 0));

    sample.valid = true;
    sample.radiance = max(rayData.rgb, 0.0);
    sample.rayDir = DecodeOcta(rayMeta.xy);
    sample.sourceUv = sourceUv;
    sample.sourcePosition = ReconstructWorldPos(scene, sourceUv, sourceDepth);
    sample.sourceNormal = normalize(DecodeGIWorldNormal(normalTex, pointSampler, sourceUv));
    return sample;
}

float ComputeRayFeatureWeight(
    SparseRaySample sample,
    float3 probePosition,
    float3 probeNormal,
    float probePlaneThreshold)
{
    if (!sample.valid)
        return 0.0;

    float normalWeight = saturate((dot(probeNormal, sample.sourceNormal) - 0.8) / 0.2);
    float planeDistance = abs(dot(probeNormal, sample.sourcePosition - probePosition));
    float planeWeight = saturate(1.0 - planeDistance / probePlaneThreshold);
    return normalWeight * planeWeight;
}

struct ReservoirState
{
    bool valid;
    SparseRaySample sample;
    float weightSum;
};

void InitReservoirState(out ReservoirState reservoir)
{
    reservoir.valid = false;
    reservoir.sample = MakeInvalidSparseRaySample();
    reservoir.weightSum = 0.0;
}

float ReservoirRandom(uint frameIndex, uint2 probeCoord, uint candidateIndex)
{
    uint packedCoord = probeCoord.x ^ hash_u32(probeCoord.y + 0x9e3779b9u);
    uint seed = hash_u32(frameIndex ^ hash_u32(packedCoord ^ hash_u32(candidateIndex + 0x85ebca6bu)));
    return (float(seed) + 0.5) / 4294967296.0;
}

void ReservoirAddSample(
    inout ReservoirState reservoir,
    SparseRaySample sample,
    float candidateWeight,
    float randomValue)
{
    if (!sample.valid || candidateWeight <= 0.0)
        return;

    reservoir.weightSum += candidateWeight;
    if (!reservoir.valid || randomValue * reservoir.weightSum <= candidateWeight)
    {
        reservoir.valid = true;
        reservoir.sample = sample;
    }
}

SparseRaySample LoadStoredReservoirSample(
    Texture2D<float4> reservoirDataTex,
    Texture2D<float4> reservoirMetaTex,
    uint2 probeCoord)
{
    SparseRaySample sample = MakeInvalidSparseRaySample();
    float4 reservoirData = reservoirDataTex.Load(int3(probeCoord, 0));
    if (reservoirData.a <= 0.0)
        return sample;

    float4 reservoirMeta = reservoirMetaTex.Load(int3(probeCoord, 0));
    sample.valid = true;
    sample.radiance = max(reservoirData.rgb, 0.0);
    sample.rayDir = DecodeOcta(reservoirMeta.xy);
    return sample;
}

SparseRaySample LoadHistoryReservoirSample(
    ParameterBlock<PerScene> scene,
    Texture2D<float4> reservoirDataTex,
    Texture2D<float4> reservoirMetaTex,
    Texture2D<float2> reservoirSourceTex,
    Texture2D<float> historyDepthTex,
    Texture2D<float4> historyNormalTex,
    SamplerState pointSampler,
    uint2 probeCoord,
    out float weightSum,
    out float accumRatio)
{
    SparseRaySample sample = MakeInvalidSparseRaySample();
    float4 reservoirData = reservoirDataTex.Load(int3(probeCoord, 0));
    float4 reservoirMeta = reservoirMetaTex.Load(int3(probeCoord, 0));

    weightSum = max(reservoirMeta.z, 0.0);
    accumRatio = saturate(reservoirMeta.w);

    if (reservoirData.a <= 0.0)
        return sample;

    float2 sourceUv = reservoirSourceTex.Load(int3(probeCoord, 0)).xy;
    float sourceDepth = historyDepthTex.SampleLevel(pointSampler, sourceUv, 0).x;
    if (sourceDepth <= 0.0000001)
        return sample;

    sample.valid = true;
    sample.radiance = max(reservoirData.rgb, 0.0);
    sample.rayDir = DecodeOcta(reservoirMeta.xy);
    sample.sourceUv = sourceUv;
    sample.sourcePosition = ReconstructPreviousWorldPos(scene, sourceUv, sourceDepth);
    sample.sourceNormal = normalize(
        DecodeGBufferNormal(historyNormalTex.SampleLevel(pointSampler, sourceUv, 0).xyz)
    );
    return sample;
}

void StoreReservoirSample(
    RWTexture2D<float4> outReservoirDataTex,
    RWTexture2D<float4> outReservoirMetaTex,
    RWTexture2D<float2> outReservoirSourceTex,
    uint2 probeCoord,
    ReservoirState reservoir,
    float accumRatio)
{
    if (!reservoir.valid)
    {
        outReservoirDataTex[probeCoord] = 0;
        outReservoirMetaTex[probeCoord] = 0;
        outReservoirSourceTex[probeCoord] = 0;
        return;
    }

    outReservoirDataTex[probeCoord] = float4(reservoir.sample.radiance, 1.0);
    outReservoirMetaTex[probeCoord] = float4(
        EncodeOcta(normalize(reservoir.sample.rayDir)),
        reservoir.weightSum,
        saturate(accumRatio)
    );
    outReservoirSourceTex[probeCoord] = reservoir.sample.sourceUv;
}

static const float kProbeAccumulationMaxFrames = 48.0;

float EncodeProbeAccumulationFrames(float frames)
{
    return saturate(frames / kProbeAccumulationMaxFrames);
}

float DecodeProbeAccumulationFrames(float normalizedFrames)
{
    return max(saturate(normalizedFrames) * kProbeAccumulationMaxFrames, 1.0);
}

float Min4(float4 value)
{
    return min(min(value.x, value.y), min(value.z, value.w));
}

uint ComputeAdaptiveRayCountFromHistory(float minAccumRatio, float lowRayCount, float maxRayCount, float stableAccumFrames)
{
    float clampedLowRayCount = max(min(lowRayCount, maxRayCount), 1.0);
    float clampedMaxRayCount = max(max(lowRayCount, maxRayCount), 1.0);
    float clampedStableFrames = max(stableAccumFrames, 1.0);
    float accumFrames = saturate(minAccumRatio) * kProbeAccumulationMaxFrames;
    float t = saturate(accumFrames / clampedStableFrames);
    return (uint)clamp(round(lerp(clampedMaxRayCount, clampedLowRayCount, t)), clampedLowRayCount, clampedMaxRayCount);
}

float GetViewDepth(ParameterBlock<PerScene> scene, float3 positionWS)
{
    return abs(mul(scene.camera.view, float4(positionWS, 1.0)).z);
}

float GetAdaptivePlaneDistanceThreshold(ParameterBlock<PerScene> scene, float distanceScale, float3 positionWS)
{
    return max(distanceScale * GetViewDepth(scene, positionWS), 1e-4);
}

float3 ReconstructWorldPos(ParameterBlock<PerScene> scene, float2 uv, float depth)
{
    float4 posWS4 = mul(scene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, depth, 1.0));
    return posWS4.xyz / posWS4.w;
}

float3 ReconstructPreviousWorldPos(ParameterBlock<PerScene> scene, float2 uv, float prevDepth)
{
    float4 posWS4 = mul(scene.camera.invPreviousViewProjection, float4(uv * 2.0 - 1.0, prevDepth, 1.0));
    return posWS4.xyz / posWS4.w;
}

bool EvaluateProbeDisocclusion(
    ParameterBlock<PerScene> scene,
    Texture2D<float2> motionVectorTex,
    Texture2D<float> historyDepthTex,
    Texture2D<float4> historyNormalTex,
    SamplerState pointSampler,
    float4 rtSize,
    uint2 probeCoord,
    float2 probeUv,
    float3 probePosition,
    float3 probeNormal,
    float probePlaneThreshold,
    out uint2 historyProbeCoord)
{
    float2 motion = motionVectorTex.SampleLevel(pointSampler, probeUv, 0).xy;
    float2 prevUV = probeUv - motion;

    historyProbeCoord = 0;
    bool isOffScreen = prevUV.x < 0.0 || prevUV.x >= 1.0 || prevUV.y < 0.0 || prevUV.y >= 1.0;
    if (isOffScreen)
        return true;

    historyProbeCoord = GetSHProbeCoordFromFullResUv(prevUV, rtSize.xy);
    uint2 historyProbeFullResCoord = GetSHProbeSourceCoord(historyProbeCoord, rtSize.xy);
    float2 historyProbeUv = GetFullResUv(historyProbeFullResCoord, rtSize);
    float historyDepth = historyDepthTex.SampleLevel(pointSampler, historyProbeUv, 0).x;
    if (historyDepth <= 0.0000001)
        return true;

    float3 historyPosition = ReconstructPreviousWorldPos(scene, historyProbeUv, historyDepth);
    float3 historyNormal = normalize(
        DecodeGBufferNormal(historyNormalTex.SampleLevel(pointSampler, historyProbeUv, 0).xyz)
    );

    float historyPlaneDistance = length(historyPosition - probePosition);
    return historyPlaneDistance > probePlaneThreshold || dot(probeNormal, historyNormal) < 0.9;
}

float2 OctaSignNotZero(float2 value)
{
    return float2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeOcta(float3 direction)
{
    direction /= max(abs(direction.x) + abs(direction.y) + abs(direction.z), 1e-6);
    float2 oct = direction.xy;
    if (direction.z < 0.0)
    {
        oct = (1.0 - abs(oct.yx)) * OctaSignNotZero(oct);
    }
    return oct * 0.5 + 0.5;
}

float3 DecodeOcta(float2 encoded)
{
    float2 oct = encoded * 2.0 - 1.0;
    float3 direction = float3(oct.x, oct.y, 1.0 - abs(oct.x) - abs(oct.y));
    if (direction.z < 0.0)
    {
        direction.xy = (1.0 - abs(direction.yx)) * OctaSignNotZero(direction.xy);
    }
    return normalize(direction);
}
