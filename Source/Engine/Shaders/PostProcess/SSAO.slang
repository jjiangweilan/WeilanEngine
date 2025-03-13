#include "Utils/VertexFullScreen.hlsl"
import Library.NormalReconstruction;
import Attributes;
import Library.PerScene;

#define GTAO_SLICE_COUNT 8
#define GTAO_DIRECTION_SAMPLE_COUNT 4
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
    float strength;
    float scaling;
    float falloff;
    float bias;

    Sampler2D depthTex;
};

ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GTAOParameter> perMaterial;

float3 GetPostionVS(float3 ndcPos)
{
    float4 posVS = mul(perScene.invProjection, float4(ndcPos, 1.0));
    return posVS.xyz / posVS.w;
}

float UpdateHorizon(VertexOutput input, float2 omega, int side, float3 posVS, float3 viewVS)
{
    float cHorizonCos = -1;
    for (uint sampleIndex = 1; sampleIndex <= GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
    {
        float s = float(sampleIndex) / float(GTAO_DIRECTION_SAMPLE_COUNT);
        float2 scaling = perMaterial.rtSize.zw * perMaterial.scaling;
        float2 sTexCoord = input.uv + side * s * scaling * float2(omega[0], -omega[1]) / posVS.z;
        float sDepth = perMaterial.depthTex.SampleLevel(sTexCoord, 0).x;
        float3 sPosV = GetPostionVS(float3(sTexCoord * 2 - 1, sDepth));

        if(any(sPosV != posVS))
        {
            float distSqr = dot(sPosV - posVS, sPosV - posVS);
            float distFalloff = clamp(distSqr * perMaterial.falloff, 0, 1);
            float horizonCos = lerp(dot(normalize(sPosV - posVS), viewVS), -1, distFalloff);
            cHorizonCos = max(cHorizonCos, horizonCos);
        }
        else
            cHorizonCos = -2;
    }

    return cHorizonCos;
}

float GTAO(VertexOutput input)
{
    float3 posVS;
    float3 normalVS = NormalReconstructionMedium(perMaterial.depthTex, input.uv, perMaterial.rtSize.zw, perScene.cameraZBufferParams, perScene.invProjection, posVS);
    float3 viewVS = normalize(-posVS);

    float visibility = 0;

    for (int slice = 0; slice < GTAO_SLICE_COUNT; ++slice)
    {
        float phi = GTAO_PI * float(slice) / float(GTAO_SLICE_COUNT);

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

        if (any(cHorizonCos == float2(-2)))
        {
            visibility += 1;
        }
        else
        {
            float2 hSide = n + clamp(float2(-1, 1) * acos(cHorizonCos) - n, -GTAO_PI_HALF, GTAO_PI_HALF);
            float2 finalSide = cosN + 2 * hSide * sin(n) - cos(2 * hSide - n);
            visibility += 0.25 * projNormalLength * (finalSide.x + finalSide.y);
        }
    }

    visibility /= GTAO_SLICE_COUNT;
    return clamp(visibility * perMaterial.strength, 0, 1);
}
#endif

[shader("fragment")]
float4 FragmentMain(VertexOutput input): SV_Target
{
if (aoType == SSAOType.GTAO)
{
    return GTAO(input);
}
else
    return 1;
}
