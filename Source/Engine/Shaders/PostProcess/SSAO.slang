#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;

static extern const bool DEBUG_show_normal_reconstruction = false;

#define APPLY_NOISE_OFFSET 1
#define GTAO_SLICE_COUNT 8
#define GTAO_DIRECTION_SAMPLE_COUNT 4
#define GTAO_PI 3.1415926f
#define GTAO_PI_HALF 1.5707963267f
#define GTAO_PI_2 6.283185307f

#define NORMAL_RECONSTRUCTION_INPLACE 1

// It seems there is an un-normalized term which is missing from the original GTAO paper
// Sims proposed a different way to normalize the final result which is described below
// using this can help eliminate unwanted occlusion
// https://x.com/BenSimsTech/status/1936667581060182395
#define NORMALIZATION_CORRECTION 1

enum SSAOType
{
    GTAO, CrysisAO, AlchmeyAO
}
static extern const SSAOType aoType = SSAOType.GTAO;

#if 1 // GTAO
struct GTAOParameter
{
    float4 rtSize;
    float2 debugPoint;
    float strength;
    float scaling;
    float falloff;
    float bias;
    float oneMeterPixelSize; // one meter object in pixel size when it's at position with view space z == 1

    Texture2D depthTex;
    RWTexture2D<float4> debugTex;
    Texture2D<float4> ignNoise;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GTAOParameter> perMaterial;

static float4 fragCoord;

float3 GetPostionVS(float3 ndcPos)
{
    float4 posVS = mul(perScene.camera.invProjection, float4(ndcPos, 1.0));
    return posVS.xyz / posVS.w;
}

static float gDebugMaxSamplePointDistance = 0;

float UpdateHorizon(VertexOutput input, float2 omega, int side, float3 posVS, float3 viewVS)
{
    float cHorizonCos = -1;

    float offsetDelta = perMaterial.scaling / (GTAO_DIRECTION_SAMPLE_COUNT - 1) * perMaterial.oneMeterPixelSize / posVS.z * side;
    for (uint sampleIndex = 1; sampleIndex <= GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
    {
        float offset = offsetDelta * sampleIndex;
        float2 sampleOffset = offset * float2(omega[0], -omega[1]);
        float2 sTexCoord = input.uv + sampleOffset * perMaterial.rtSize.zw;

        // Sample view position
        float sDepth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, sTexCoord, 0).x;
        float3 sPosV = GetPostionVS(float3(sTexCoord * 2 - 1, sDepth));

        // Calculate falloff
        float distSqr = dot(sPosV - posVS, sPosV - posVS);
        float distFalloff = clamp(distSqr * perMaterial.falloff, 0, 1);

        float horizonCos = lerp(dot(normalize(sPosV - posVS), viewVS), -1, distFalloff);
        cHorizonCos = max(cHorizonCos, horizonCos);
    }

    return cHorizonCos;
}

float GTAO(VertexOutput input)
{
    float3 posVS;

#if NORMAL_RECONSTRUCTION_INPLACE
    float3 normalVS = NormalReconstructionHigh2(perMaterial.depthTex, perMaterial.sampler_point_clamp, input.uv, perMaterial.rtSize.zw, perScene.camera.cameraZBufferParams, perScene.camera.invProjection, posVS);
#endif
    float3 viewVS = normalize(-posVS);

    float visibility = 0;
#if NORMALIZATION_CORRECTION
    float normalizationCorrection = 0.0f;
#endif

    float sampleSliceRotationOffset = 0.1f;
#if APPLY_NOISE_OFFSET
    float noise = perMaterial.ignNoise.Load(int3((int2)(input.position.xy % 32.0f), 0)).x;
#endif
    for (int slice = 0; slice < GTAO_SLICE_COUNT; ++slice)
    {
        float phi = GTAO_PI * float(slice) / float(GTAO_SLICE_COUNT);

#if APPLY_NOISE_OFFSET
        phi += noise;
#endif

        float2 omega = normalize(float2(cos(phi), sin(phi)));

        float3 dirV = float3(omega.x, omega.y, 0);
        float3 orthoDirectionV = dirV - dot(dirV, viewVS) * viewVS;
        float3 axisV = normalize(cross(orthoDirectionV, viewVS));
        float3 projNormalV = normalVS - axisV * dot(normalVS, axisV);

        const float projNormalLength = length(projNormalV);

        float signN = sign(dot(orthoDirectionV, projNormalV));
        float cosN = saturate(dot(projNormalV, viewVS) / projNormalLength);
        float n = signN * acos(cosN);

        float2 cHorizonCos = float2(-1.0f, -1.0f);
        cHorizonCos.x = UpdateHorizon(input, omega, -1, posVS.xyz, viewVS); // GTAO_DIRECTION_SAMPLE_COUNT samples
        cHorizonCos.y = UpdateHorizon(input, omega, 1, posVS.xyz, viewVS);

        float2 angle = acos(cHorizonCos);

        // clampping the value to prevent it exceeding normal's semi-sphere
        float2 hSide = n + clamp(float2(-1, 1) * angle  - n, -GTAO_PI_HALF, GTAO_PI_HALF);

        float2 finalSide;
        finalSide.x = cosN + 2 * hSide.x * sin(n) - cos(2 * hSide.x - n);
        finalSide.y = cosN + 2 * hSide.y * sin(n) - cos(2 * hSide.y - n);
        visibility += 0.25 * projNormalLength * (finalSide.x + finalSide.y);

        #if NORMALIZATION_CORRECTION
        normalizationCorrection += projNormalLength * (n * sin(n) + cosN);
        #endif
    }

#if NORMALIZATION_CORRECTION
    visibility /= normalizationCorrection;
#else
    visibility /= GTAO_SLICE_COUNT;
#endif

    if (DEBUG_show_normal_reconstruction)
    {
        perMaterial.debugTex[int2(input.uv * perMaterial.rtSize.xy)] = float4(normalVS, 1.0);
    }

    visibility = pow(visibility, perMaterial.strength);
    // visibility = (1 - visibility) * perMaterial.strength;
    return saturate(visibility);
}
#endif

[shader("fragment")]
float4 FragmentMain(VertexOutput input, float4 screenPos : SV_Position): SV_Target
{
if (aoType == SSAOType.GTAO)
{
    return GTAO(input);
}
else
    return 1;
}
