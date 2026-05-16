#include "s2h/s2h.hlsl"
#include "s2h/s2h_3d.hlsl"

#include "Utils/VertexFullScreen.hlsl"

import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;

#define SSIL_PI 3.14159265359f
#define SSIL_HALF_PI 1.57079632679f
#define SSIL_TWO_PI 6.28318530718f

// 0 or 1
#define DEBUG_DRAW 0

// 0 or 1
#define SSIL_USE_SHARED_SAMPLES 0

#define SSIL_AO_ONLY 1

#define SSIL_SHARED_SAMPLES_SUBSLICE_COUNT 2

#if SSIL_USE_SHARED_SAMPLES
    #define SSIL_SLICE_COUNT 8 // 2x2 shared samples * 2 subslices for each thread
#else
    #define SSIL_SLICE_COUNT perMaterial.sliceCount
#endif

struct SSILParameter
{
    float4 rtSize;
    float strength;
    float thickness;
    float radius;
    float sliceCount;
    float sampleCount;
    float jitterScale;
    float sampleJitterScale;
    float debug_ssilOutput;
    float oneMeterPixelSize;
    float2 debugPoint;

    float4 fullResSize;

    Texture2D<uint2> geometryTex;
    Texture2D<uint> geometryCoordOffsetTex;
    Texture2D colorTex;
    Texture2D albedoTex;
    Texture2D<float2> blueNoise;
    RWTexture2D<float4> outSsilTex;
    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_repeat;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<SSILParameter> perMaterial;

struct SSILPassData
{
__init(float2 uv)
    {
        uint2 pixelCoord = uint2(uv * perMaterial.rtSize.xy);
        uint2 halfPixel = min(pixelCoord, uint2(perMaterial.rtSize.xy) - 1);
        uint2 packed = perMaterial.geometryTex.Load(int3(halfPixel, 0));
        float depth = asfloat(packed.x);
        this.normalWS = UnpackNormalOctahedral16(packed.y);
        uint offsetPacked = perMaterial.geometryCoordOffsetTex.Load(int3(halfPixel, 0)).x;
        uint2 offset = uint2(offsetPacked & 1u, (offsetPacked >> 1u) & 1u);
        uint2 fullCoord = halfPixel * 2 + offset;
        float2 geometryUv = (float2(fullCoord) + 0.5) * perMaterial.fullResSize.zw;
        float4 posWS = mul(perScene.camera.invNDCToWorld, float4(geometryUv * 2 - 1, depth, 1.0));
        float4 posVS = mul(perScene.camera.invProjection, float4(geometryUv * 2 - 1, depth, 1.0));

        this.positionWS = posWS.xyz / posWS.w;
        this.positionVS = posVS.xyz / posVS.w;
        this.depth = depth;
    }

    float3 positionWS;
    float3 positionVS;
    float3 normalWS;

    float depth;
};

float3 GetNormalVS(float2 uv)
{
    uint2 pixelCoord = uint2(uv * perMaterial.rtSize.xy);
    uint2 halfPixel = min(pixelCoord, uint2(perMaterial.rtSize.xy) - 1);
    uint2 packed = perMaterial.geometryTex.Load(int3(halfPixel, 0));
    float3 normalWS = UnpackNormalOctahedral16(packed.y);
    return mul((float3x3)perScene.camera.view, normalWS);
}




#if DEBUG_DRAW
static Context3D context;
static ContextGather debugGatherContext;
#endif

#if SSIL_USE_SHARED_SAMPLES
groupshared float3 g_sharedSamples[64];
#endif

float2 SSILFrameJitter(uint frameIndex)
{
    // Irrational increments avoid repeating the same blue-noise phase every frame.
    return frac(float2(0.754877666f, 0.569840291f) * float(frameIndex + 1));
}

float2 SSILBlueNoise(uint2 pixelCoord, uint frameIndex)
{
    uint2 tileOffset = uint2(frameIndex * 47u, frameIndex * 17u);
    float2 noise = perMaterial.blueNoise[(pixelCoord + tileOffset) % 128];
    return frac(noise + SSILFrameJitter(frameIndex));
}

float SSILHash(uint2 pixelCoord, uint frameIndex, uint sliceIndex, uint sampleIndex, uint dimension)
{
    uint seed = pixelCoord.x * 0x9e3779b9u;
    seed ^= pixelCoord.y * 0x85ebca6bu;
    seed ^= frameIndex * 0xc2b2ae35u;
    seed ^= sliceIndex * 0x27d4eb2fu;
    seed ^= sampleIndex * 0x165667b1u;
    seed ^= dimension * 0xd3a2646cu;
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16;
    return float(seed & 0x00ffffffu) / 16777216.0f;
}

void BuildSurfaceBasis(float3 normalVS, float3 viewVS, out float3 tangentVS, out float3 binormalVS)
{
    float3 viewTangent = viewVS - normalVS * dot(viewVS, normalVS);
    if (dot(viewTangent, viewTangent) < 1e-4f)
    {
        float3 fallback = abs(normalVS.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
        viewTangent = fallback - normalVS * dot(fallback, normalVS);
    }

    tangentVS = normalize(viewTangent);
    binormalVS = normalize(cross(normalVS, tangentVS));
}

float4 SSIL(float2 uv, float2 screenPos, uint groupIndex, bool debug)
{
    SSILPassData passData = SSILPassData(uv);

    // main algorithm
    float3 posVS = passData.positionVS;
    float fragDepth = passData.depth;
    
    if (fragDepth < 0.0000001)
    {
        return 0;
    }

    float3 normalVS = normalize(GetNormalVS(uv));
    float3 viewVS = normalize(-posVS);
    float3 posDirVS = normalize(posVS);

    float3 indirectLight = 0;
    float ao = 0;
    float totalWeight = 0;

    float3 tangentVS = float3(1,0,0);
    float3 binormaVS = float3(0,1,0);
    BuildSurfaceBasis(normalVS, viewVS, tangentVS, binormaVS);

    uint frameIndex = (uint)perScene.scene.frameIndex;
    uint2 pixelCoord = uint2(screenPos);
    float2 noise = SSILBlueNoise(pixelCoord, frameIndex);

#if DEBUG_DRAW
    if (debug)
    {
        float3 posWS = mul(perScene.camera.invView, float4(posVS, 1.0)).xyz;
        s2h_drawSphereWS(context, posWS, float4(1,0,0,1), 0.1);
        // Draw Tangent (Red) and Binormal (Blue) basis
        s2h_drawLineWS(context, posWS, mul(perScene.camera.invView, float4(posVS + tangentVS * perMaterial.radius, 1.0)).xyz, float4(1, 0, 0, 1), 0.02f);
        s2h_drawLineWS(context, posWS, mul(perScene.camera.invView, float4(posVS + binormaVS * perMaterial.radius, 1.0)).xyz, float4(0, 0, 1, 1), 0.02f);
    }
#endif

    const float Nbits = 32;
    uint lastBits = 0; // For debug display
    float lightSamples = 0;

#if SSIL_USE_SHARED_SAMPLES
    uint sliceIndex = groupIndex % 4;
    for(uint subsliceIndex = 0; subsliceIndex < SSIL_SHARED_SAMPLES_SUBSLICE_COUNT; ++subsliceIndex)
#else
    for(uint sliceIndex = 0; sliceIndex < SSIL_SLICE_COUNT; ++sliceIndex)
#endif
    {
        uint bits_i = 0;

#if SSIL_USE_SHARED_SAMPLES
        float sliceJitter = frac(noise.x + SSILHash(pixelCoord, frameIndex, sliceIndex, subsliceIndex, 0u));
        float phi = SSIL_PI * float(sliceIndex + 0.5 * subsliceIndex + sliceJitter * perMaterial.jitterScale) / SSIL_SLICE_COUNT;
#else
        float sliceJitter = frac(noise.x + SSILHash(pixelCoord, frameIndex, sliceIndex, 0u, 0u));
        float phi = SSIL_PI * float(sliceIndex + sliceJitter * perMaterial.jitterScale) / SSIL_SLICE_COUNT;
#endif

        float2 sampleDir = normalize(float2(cos(phi), -sin(phi)));
        float3 axis = normalize(tangentVS * sampleDir.x + binormaVS * sampleDir.y);
        float sampleCount = perMaterial.sampleCount;

        float3 dirDelta = axis * perMaterial.radius / sampleCount;

        for (float direction = 0; direction < 2; ++direction)
        {
            float directionFixed = direction * 2 - 1;

            for (float sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex)
            {
                float3 samplePosVS = posVS + dirDelta * sampleIndex * directionFixed;
                float4 sampleNDC = mul(perScene.camera.projection, float4(samplePosVS, 1.0));
                float2 sampleUV = sampleNDC.xy / sampleNDC.w * 0.5 + 0.5;
                float2 sampleUvDir = sampleUV - uv;
                float sampleUvDirLenSq = dot(sampleUvDir, sampleUvDir);
                if (sampleUvDirLenSq > 0.0)
                {
                    float sampleJitter = frac(noise.y + SSILHash(pixelCoord, frameIndex, sliceIndex, (uint)sampleIndex, 1u));
                    sampleUV += (sampleJitter * 2 - 1)
                        * sampleUvDir * rsqrt(sampleUvDirLenSq)
                        * perMaterial.rtSize.zw
                        * perMaterial.sampleJitterScale;
                }

                if (any(sampleUV > 1.0) || any(sampleUV < 0.0))
                    continue;

                uint2 sampleHalfPixel = min(uint2(sampleUV * perMaterial.rtSize.xy), uint2(perMaterial.rtSize.xy) - 1);
                uint2 samplePacked = perMaterial.geometryTex.Load(int3(sampleHalfPixel, 0));
                float depth = asfloat(samplePacked.x);
                uint sampleOffsetPacked = perMaterial.geometryCoordOffsetTex.Load(int3(sampleHalfPixel, 0)).x;
                uint2 sampleOffset = uint2(sampleOffsetPacked & 1u, (sampleOffsetPacked >> 1u) & 1u);
                uint2 sampleFullCoord = sampleHalfPixel * 2 + sampleOffset;
                float2 sampleGeometryUv = (float2(sampleFullCoord) + 0.5) * perMaterial.fullResSize.zw;
#if SSIL_USE_SHARED_SAMPLES
                g_sharedSamples[groupIndex] = float3(sampleUV, depth);
                GroupMemoryBarrierWithGroupSync();
#endif

#if SSIL_USE_SHARED_SAMPLES
                for (int sharedSamplesSubgroupIndex = 0; sharedSamplesSubgroupIndex < 4; sharedSamplesSubgroupIndex++) 
                {
                    uint sharedSampleOffset = groupIndex / 4 * 4;
                    float3 sharedSamples = g_sharedSamples[sharedSampleOffset + sharedSamplesSubgroupIndex]; 
                    float2 sampleUV = sharedSamples.xy;
                    float depth = sharedSamples.z;
#endif

                    float4 frontVS = mul(perScene.camera.invProjection, float4(sampleGeometryUv * 2 - 1, depth, 1.0));
                    frontVS.xyz /= frontVS.w;

                    float3 backVS = frontVS.xyz + posDirVS * perMaterial.thickness;
                    float3 frontAngleDir = normalize(frontVS.xyz - posVS);
                    float3 backAngleDir = normalize(backVS - posVS);
                    float frontAngle = acos(clamp(dot(normalVS, frontAngleDir), 0, SSIL_HALF_PI) * select(dot(frontAngleDir, axis) < -1, -1, 1));
                    float backAngle = acos(clamp(dot(normalVS, backAngleDir), 0, SSIL_HALF_PI) * select(dot(backAngleDir, axis) < -1, -1, 1));

                    float2 minMaxAngle = frontAngle > backAngle ? float2(backAngle, frontAngle) : float2(frontAngle, backAngle);

                    uint a = (uint)(floor((minMaxAngle.x + SSIL_HALF_PI) / SSIL_PI * Nbits + 0.5));
                    uint b = (uint)(ceil((minMaxAngle.y - minMaxAngle.x) / SSIL_PI * Nbits - 0.5));

                    uint bits_j = (uint)pow(2, b) - 1 << a;

                    // GI
#if SSIL_AO_ONLY == 0
                    float3 lighting = perMaterial.colorTex.SampleLevel(perMaterial.sampler_point_clamp, sampleUV, 0).xyz;
                    float3 normal = GetNormalVS(sampleUV);
                    float3 l = normalize(frontVS.xyz - posVS);
                    float giWeight = countbits(bits_j & ~bits_i) / Nbits * dot(normal, l) * dot(normalVS, -l);
                    indirectLight = indirectLight + giWeight * lighting;
                    lightSamples += 1;
#endif

                    // ao
                    bits_i = bits_i | bits_j;

#if DEBUG_DRAW
                    if (debug)
                    {
                        // Draw front (green) and back (yellow) thickness indicator
                        s2h_drawSphereWS(context, mul(perScene.camera.invView, float4(frontVS.xyz, 1.0)).xyz, float4(0,1,0,1), 0.05f);
                        s2h_drawSphereWS(context, mul(perScene.camera.invView, float4(backVS.xyz, 1.0)).xyz, float4(1,1,0,0.5), 0.03f);
                    }
#endif

#if SSIL_USE_SHARED_SAMPLES
                }
#endif
            }
        }

        lastBits = bits_i;
        ao += 1 - countbits(bits_i) / Nbits;
    }
    float sliceCount = SSIL_SLICE_COUNT;
    ao /= sliceCount;
#if SSIL_AO_ONLY == 0
    indirectLight /= sliceCount;
#endif


#if DEBUG_DRAW
    if (debug)
    {
        s2h_setCursor(debugGatherContext, float2(10, 30));

        debugGatherContext.textColor = float4(1, 1, 0, 1);
        s2h_printTxt(debugGatherContext, _S, _S, _I, _L);
        s2h_printTxt(debugGatherContext, _SPACE, _D, _E, _B, _U, _G);
        s2h_printLF(debugGatherContext);

        debugGatherContext.textColor = float4(1, 1, 1, 1);
        s2h_printTxt(debugGatherContext, _R, _A, _D, _I, _U, _S);
        s2h_printTxt(debugGatherContext, _COLON);
        s2h_printSpace(debugGatherContext, 1.0f);
        s2h_printFloat(debugGatherContext, perMaterial.radius);
        s2h_printLF(debugGatherContext);

        s2h_printTxt(debugGatherContext, _T, _H, _I, _C, _K);
        s2h_printTxt(debugGatherContext, _N, _E, _S, _S, _COLON);
        s2h_printSpace(debugGatherContext, 1.0f);
        s2h_printFloat(debugGatherContext, perMaterial.thickness);
        s2h_printLF(debugGatherContext);

        s2h_printTxt(debugGatherContext, _A, _O, _COLON);
        s2h_printSpace(debugGatherContext, 1.0f);
        s2h_printFloat(debugGatherContext, ao);
        s2h_printLF(debugGatherContext);

        s2h_printTxt(debugGatherContext, _M, _A, _S, _K);
        s2h_printTxt(debugGatherContext, _SPACE, _L, _A, _S, _T, _COLON);
        s2h_printSpace(debugGatherContext, 1.0f);
        s2h_printHex(debugGatherContext, lastBits);
    }
#endif

#if SSIL_AO_ONLY == 0
    return perMaterial.strength * max(float4(indirectLight, ao), 0);
#else
    ao = pow(saturate(ao), perMaterial.strength);
    return float4(ao, ao, ao, ao);
#endif
}

[shader("compute")]
    [numthreads(64, 1, 1)]
void ComputeMain(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex, uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = MortonDecode8x8(groupIndex) + groupID.xy * 8;

    if (pixelCoord.x >= perMaterial.rtSize.x || pixelCoord.y >= perMaterial.rtSize.y)
        return;

    float2 uv = (float2(pixelCoord.xy) + 0.5f) * perMaterial.rtSize.zw;
    float2 screenPos = float2(pixelCoord.xy);

    float4 ssil = SSIL(uv, screenPos, groupIndex, false);
#if DEBUG_DRAW
    float3 dir = perScene.UvToWorldRay(uv);
    s2h_init(context, perScene.camera.position.xyz, dir);
    s2h_init(debugGatherContext, screenPos);
    SSIL(perMaterial.debugPoint.xy, screenPos, groupIndex, true);

    ssil.xyz = lerp(ssil.xyz, context.dstColor.xyz, context.dstColor.a);
    ssil.xyz = lerp(ssil.xyz, debugGatherContext.dstColor.xyz, debugGatherContext.dstColor.a);
#endif

    perMaterial.outSsilTex[pixelCoord.xy] = ssil;
}
