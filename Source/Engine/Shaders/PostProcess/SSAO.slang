#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;

#define GTAO_SLICE_COUNT 4
#define GTAO_DIRECTION_SAMPLE_COUNT 6
#define GTAO_PI 3.1415926f
#define GTAO_PI_HALF 1.5707963267f
#define GTAO_PI_2 6.283185307f

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

    Texture2D depthTex;
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
    float2 offsetScaling = perMaterial.scaling * side * float2(omega[0], -omega[1]);
    for (uint sampleIndex = 1; sampleIndex <= GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
    {
        float2 sampleOffset = offsetScaling * sampleIndex;
        float2 sTexCoord = input.uv + sampleOffset * perMaterial.rtSize.zw ;// / posVS.z;

        float sDepth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, sTexCoord, 0).x;
        float3 sPosV = GetPostionVS(float3(sTexCoord * 2 - 1, sDepth));

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
    float3 normalVS = NormalReconstructionMedium(perMaterial.depthTex, perMaterial.sampler_point_clamp, input.uv, perMaterial.rtSize.zw, perScene.camera.cameraZBufferParams, perScene.camera.invProjection, posVS);
    float3 viewVS = normalize(-posVS);

    float visibility = 0;

    for (int slice = 0; slice < GTAO_SLICE_COUNT; ++slice)
    {
        input.position.x + fmod(floor(input.position.y), 4) * 4;
        float phi = GTAO_PI * (input.position.x / 4.0f + float(slice) / float(GTAO_SLICE_COUNT));

        float2 omega = float2(cos(phi), sin(phi));

        float3 dirV = float3(omega, 0);
        float3 orthoDirectionV = dirV - dot(dirV, viewVS) * viewVS;
        float3 axisV = normalize(cross(dirV, viewVS));
        float3 projNormalV = normalVS - axisV * dot(normalVS, axisV);

        const float projNormalLength = length(projNormalV);

        float signN = sign(dot(orthoDirectionV, projNormalV));
        float cosN = clamp(dot(projNormalV, viewVS) / projNormalLength, 0.0f, 1.0f);
        float n = signN * acos(cosN);

        float2 cHorizonCos = float2(-1.0f, -1.0f);
        cHorizonCos.x = UpdateHorizon(input, omega, -1, posVS, viewVS);
        cHorizonCos.y = UpdateHorizon(input, omega, 1, posVS, viewVS);

        float2 hSide = n + clamp(float2(-1, 1) * acos(cHorizonCos) - n, -GTAO_PI_HALF, GTAO_PI_HALF);
        float2 finalSide = cosN + 2 * hSide * sin(n) - cos(2 * hSide - n);
        visibility += 0.25 * projNormalLength * (finalSide.x + finalSide.y);
    }

    visibility /= GTAO_SLICE_COUNT;

    visibility = (1 - visibility) * perMaterial.strength;
    return saturate(1 - visibility);
}
#endif

[shader("fragment")]
float4 FragmentMain(VertexOutput input, float4 screenPos : SV_Position): SV_Target
{
fragCoord = screenPos;
if (aoType == SSAOType.GTAO)
{
    return GTAO(input);
}
else
    return 1;
}
