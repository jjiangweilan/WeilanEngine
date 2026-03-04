#include "s2h/s2h.hlsl"
#include "s2h/s2h_3d.hlsl"

#include "Utils/VertexFullScreen.hlsl"

import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;

#define SSIL_PI 3.14159265359f
#define SSIL_HALF_PI 1.57079632679f
#define SSIL_TWO_PI 6.28318530718f
#define DEBUG_DRAW 0

struct SSILParameter
{
    float4 rtSize;
    float strength;
    float thickness;
    float radius;
    float sliceCount;
    float sampleCount;
    float debug_ssilOutput;
    float oneMeterPixelSize;
    float2 debugPoint;

    Texture2D depthTex;
    Texture2D normalTex;
    Texture2D colorTex;
    Texture2D albedoTex;
    Texture2D<float4> ignNoise;
    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_repeat;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<SSILParameter> perMaterial;

float4 GetPositionVS(float2 uv)
{
    float depth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;
    float4 posVS = mul(perScene.camera.invProjection, float4(uv * 2 - 1, depth, 1.0));
    return float4(posVS.xyz / posVS.w, depth);
}

float3 GetNormalVS(float2 uv)
{
    float3 normalWS = perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xyz;
    normalWS = DecodeGBufferNormal(normalWS);
    return mul((float3x3)perScene.camera.view, normalWS);
}

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float randf(int x, int y) {
    float f = 1.0f;
    return modf(52.9829189 * modf(0.06711056 * float(x) + 0.00583715 * float(y), f), f);
}


#if DEBUG_DRAW
static Context3D context;
static ContextGather debugGatherContext;
#endif

float4 SSIL(float2 uv, float2 screenPos, bool debug)
{
    // main algorithm
    float4 fragPosVS = GetPositionVS(uv);
    float3 posVS = fragPosVS.xyz;
    float fragDepth = fragPosVS.w;
    
    if (fragDepth < 0.000001)
    {
        return 0;
    }

    float3 normalVS = normalize(GetNormalVS(uv));
    float3 viewVS = normalize(-posVS);
    float3 posDirVS = normalize(posVS);

    float3 indirectLight = 0;
    float ao = 0;
    float totalWeight = 0;

    float3 upVS = normalVS;
    float3 tangentVS = normalize(cross(all(upVS == float3(1, 0, 0)) ? float3(0, 1, 0) : float3(1,0,0), upVS));
    float3 binormaVS = cross(upVS, tangentVS);

    float jitter = randf((int)screenPos.x, (int)screenPos.y);

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
    for(uint sliceIndex = 0; sliceIndex < perMaterial.sliceCount; ++sliceIndex)
    {
        float phi = SSIL_PI * float(sliceIndex + jitter) / perMaterial.sliceCount;

        float2 sampleDir = normalize(float2(cos(phi), -sin(phi)));
        float3 axis = normalize(tangentVS * sampleDir.x + binormaVS * sampleDir.y);
        float sampleCount = perMaterial.sampleCount;

        float3 dirDelta = axis * perMaterial.radius / sampleCount;

        // float4 uvStart = mul(perScene.camera.projection, float4(sampleStartPosVS, 1.0));
        // float4 uvEnd = mul(perScene.camera.projection, float4(sampleEndPosVS, 1.0));
        // uvStart.xy /= uvStart.w;
        // uvStart.xy = uvStart.xy * 0.5 + 0.5;
        // uvEnd.xy /= uvEnd.w;
        // uvEnd.xy = uvEnd.xy * 0.5 + 0.5;

        uint bits_i = 0;

        for (float direction = 0; direction < 2; ++direction)
        {
            float directionFixed = direction * 2 - 1;

            for (float sampleIndex = 1; sampleIndex <= sampleCount; ++sampleIndex)
            {
                float3 samplePosVS = posVS + dirDelta * sampleIndex * directionFixed;
                float4 sampleNDC = mul(perScene.camera.projection, float4(samplePosVS, 1.0));
                float2 sampleUV = sampleNDC.xy / sampleNDC.w * 0.5 + 0.5;

                float depth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, sampleUV, 0).x;
                float4 frontVS = mul(perScene.camera.invProjection, float4(sampleUV * 2 - 1, depth, 1.0));
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
                float3 lighting = perMaterial.colorTex.SampleLevel(perMaterial.sampler_point_clamp, sampleUV, 0).xyz;
                float3 normal = GetNormalVS(sampleUV);
                float3 l = normalize(frontVS.xyz - posVS);
                indirectLight = indirectLight + countbits(bits_j & ~bits_i) / Nbits * lighting * dot(normal, l) * dot(normalVS, -l);

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
            }
        }
        lastBits = bits_i;
        ao = ao + 1 - countbits(bits_i) / Nbits;
    }

    ao = ao / perMaterial.sliceCount;
    indirectLight /= perMaterial.sliceCount;

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

    return perMaterial.strength * max(float4(indirectLight, ao), 0);
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input, float4 screenPos : SV_Position) : SV_Target
{
    float4 ssil = SSIL(input.uv, screenPos.xy, false);
#if DEBUG_DRAW
    float3 dir = perScene.UvToWorldRay(input.uv);
    s2h_init(context, perScene.camera.position.xyz, dir);
    s2h_init(debugGatherContext, screenPos.xy);
    SSIL(perMaterial.debugPoint.xy, screenPos.xy, true);

    ssil.xyz = lerp(ssil.xyz, context.dstColor.xyz, context.dstColor.a);
    ssil.xyz = lerp(ssil.xyz, debugGatherContext.dstColor.xyz, debugGatherContext.dstColor.a);
#endif

    return ssil;
}
