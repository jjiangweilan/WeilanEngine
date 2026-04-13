import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Utils.Noise;
import Library.RayTracing;
import Lighting;
import GI.GI_Common;

#define GI_WS_DEBUG 0

#if GI_WS_DEBUG
#include "../s2h/s2h.hlsl"
#include "../s2h/s2h_3d.hlsl"
#endif

struct GIParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float secondary_bounce;

    // Full-res G-buffer inputs
    Texture2D hierarchyDepth;
    Texture2D normalTex;
    Texture2D albedoTex;
    Texture2D<float4> noiseTex;
    Texture2D<float2> motionVectorTex;
    StructuredBuffer<float2> haltonSeq;

    // Quarter-res SH history (previous frame)
    Texture2D<float4> historySH0Tex;
    Texture2D<float4> historySH1Tex;
    Texture2D<float4> historySH2Tex;
    Texture2D<float> historyAccumTex;

    // Full-res history for disocclusion detection
    Texture2D<float> historyDepthTex;
    Texture2D<float4> historyNormalTex;

    // Quarter-res SH outputs (current frame)
    RWTexture2D<float4> outSH0Tex;
    RWTexture2D<float4> outSH1Tex;
    RWTexture2D<float4> outSH2Tex;
    RWTexture2D<float> outAccumTex;

#if GI_WS_DEBUG
    // Full-res debug output (xyz = color, w = depth)
    RWTexture2D<float4> outDebugTex;
    float4 debugPixel;
#endif

    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;

    RaytracingAccelerationStructure sceneBVH;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIParameter> perMaterial;

float3 GetNormalWS(float2 uv)
{
    float3 normalWS = perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xyz;
    return DecodeGBufferNormal(normalWS);
}

#define diff3(f, x, y) (((f((x) + (y))) + (f((x) - (y))) - 2.*(f((x)))) / dot(y,y))

float hash21(float2 p)
{
	float3 q = fract(float3(p.xyx) * .1031);
    q += dot(q, q.yzx + 19.19);
    return frac((q.x + q.y) * q.z);
}

//source https://www.shadertoy.com/view/Wtfczs
float bluenoise(float2 p)
{
    return 1./4. * (
          diff3(hash21, p, float2(1,0)) 
        + diff3(hash21, p, float2(0,1))
        ) + .5;
}

struct GIContext
{
    uint2 tileCoord;
    float2 fullResUv;
    float3 posWS;
    float3 normalWS;
    float3 firstRay;

    SurfaceHit hit;

    float4 sh0;
    float4 sh1;
    float4 sh2;
    float accumCount;
};

GIContext CalcGI(uint2 dispatchThreadID)
{
    GIContext context;

    uint2 tileCoord = dispatchThreadID.xy;
    context.tileCoord = tileCoord;
    uint2 quarterSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (context.tileCoord.x >= quarterSize.x || context.tileCoord.y >= quarterSize.y)
        return context;

    uint frameIndex = (uint)perScene.scene.frameIndex;
    // Select a random sub-pixel within the 4x4 tile
    uint2 offset = GetTileOffset(context.tileCoord, frameIndex, quarterSize);
    uint2 fullResCoord = context.tileCoord * 2 + offset;

    float2 fullResUv = GetFullResUv(fullResCoord, perMaterial.rtSize);// (float2(min(fullResCoord, perMaterial.rtSize.xy - 1)) + 0.5f) * perMaterial.rtSize.zw;
    context.fullResUv = fullResUv;

    // Reproject and read history SH
    float2 motion = perMaterial.motionVectorTex.SampleLevel(perMaterial.sampler_point_clamp, fullResUv, 0).xy;
    float2 prevUV = fullResUv - motion;
    uint2 prevCoord = (prevUV * perMaterial.rtSize.xy) / 2;

    float depth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, fullResUv, 1).x;
    if (depth <= 0.0000001)
    {
        context.sh0 = 0;
        context.sh1 = 0;
        context.sh2 = 0;
        context.accumCount = 0;
        return context;
    }

    float4 posWS4   = mul(perScene.camera.invNDCToWorld, float4(fullResUv * 2.0 - 1.0, depth, 1.0));
    context.posWS    = posWS4.xyz / posWS4.w;
    context.normalWS = normalize(GetNormalWS(fullResUv));

    // Combine Halton low-discrepancy sequence with blue noise for temporally stable sampling
    float2 halton    = perMaterial.haltonSeq[frameIndex % 32];
    float2 blueNoise = perMaterial.noiseTex[uint2(fullResCoord) % 128].xy;
    float2 randVal   = frac(halton + blueNoise);

    const uint totalBounce = perMaterial.secondary_bounce > 0.5 ? 2 : 1;

    float3 indirectLight     = 0;
    float3 currentThroughput = 1.0;
    float3 currentOrigin     = OffsetRay(context.posWS, context.normalWS);
    float3 currentNormal     = context.normalWS;
    float3 firstRayDir       = float3(0, 1, 0);

    for (uint bounce = 0; bounce < totalBounce; bounce++)
    {
        float sinTheta = 0;
        float2 uv = bounce == 0 ? randVal : frac(randVal + 0.6180339887);
        float3 rayDir = GetSemiSphereDirection(currentNormal, uv, sinTheta);
        sinTheta = max(sinTheta, 0.001); // Avoid division by zero

        if (bounce == 0)
            firstRayDir = rayDir;

        context.hit = GetRayHitSurface(perMaterial.sceneBVH, perScene, currentOrigin, rayDir, dispatchThreadID.xy);
        if (context.hit.hit)
        {
            indirectLight     += currentThroughput * (context.hit.emissive + context.hit.albedo * context.hit.directLight);
            currentThroughput *= context.hit.albedo;
            currentOrigin      = OffsetRay(context.hit.position, context.hit.normal);
            currentNormal      = context.hit.normal;
        }
        else
        {
            Light mainLight  = perScene.GetMainLight();
            indirectLight   += currentThroughput * Sky(currentOrigin, rayDir, mainLight.position.xyz, mainLight.lightColor.a > 0.0) * 20;
            break;
        }
    }
    context.firstRay = firstRayDir;

    indirectLight = max(indirectLight, 0.00001); // Avoid black SH which can cause issues for ZH3

    // Encode incoming radiance into Linear SH (L0 + L1)
    static const float Y00 = 0.28209479; // 1 / sqrt(4 * PI)
    static const float Y1  = 0.48860251; // sqrt(3 / (4 * PI))

    float4 shBases = float4(Y00, Y1 * firstRayDir.x, Y1 * firstRayDir.y, Y1 * firstRayDir.z);
    float4 newSH0 = shBases * indirectLight.r;
    float4 newSH1 = shBases * indirectLight.g;
    float4 newSH2 = shBases * indirectLight.b;

    // --- Disocclusion detection & history compensation ---
    float alpha = 1.0f;
    float4 finalHistSH0 = 0;
    float4 finalHistSH1 = 0;
    float4 finalHistSH2 = 0;

    float currentDepth = depth;

    // 1. Check direct reprojection (center pixel)
    bool isOffScreen = prevUV.x < 0 || prevUV.x > 1 || prevUV.y < 0 || prevUV.y > 1;

    float centerHistDepth = isOffScreen ? 0.0f : perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).x;
    float3 centerHistNormal = isOffScreen ? float3(0, 0, 0) : DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).xyz);

    bool isDisoccluded = isOffScreen;
                         // (abs(LinearEyeDepth(currentDepth, perScene.camera.cameraZBufferParams) - LinearEyeDepth(centerHistDepth, perScene.camera.cameraZBufferParams)) > 0.05f) ||
                         // (dot(context.normalWS, centerHistNormal) < 0.9f);

    if (!isDisoccluded)
    {
        // Center is valid — fast path
        finalHistSH0 = perMaterial.historySH0Tex.Load(int3(prevCoord, 0));
        finalHistSH1 = perMaterial.historySH1Tex.Load(int3(prevCoord, 0));
        finalHistSH2 = perMaterial.historySH2Tex.Load(int3(prevCoord, 0));
        float histAccum = perMaterial.historyAccumTex.Load(int3(prevCoord, 0)).x;
        context.accumCount = min(histAccum + (1.0f / 255.0f), 1.0f);
        alpha = lerp(0.04f, 1.0f, 1 - clamp((context.accumCount * 255) / 16.0f, 0.0f, 1.0f));
    }
    else
    {
        context.accumCount = 1.0f / 255.0f;
        alpha = 1.0f;
    }

    // Exponential moving average blend
    context.sh0 = lerp(finalHistSH0, newSH0, alpha);
    context.sh1 = lerp(finalHistSH1, newSH1, alpha);
    context.sh2 = lerp(finalHistSH2, newSH2, alpha);

    return context;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    GIContext context = CalcGI(dispatchThreadID.xy);

#if GI_WS_DEBUG
    // debug
    GIContext debugGIcontext = CalcGI((uint2)perMaterial.debugPixel.xy);
    Context3D debugContext;
    float3 dir = perScene.UvToWorldRay(context.fullResUv);
    s2h_init(debugContext, perScene.camera.position.xyz, dir);
    s2h_drawLineWS(debugContext, debugGIcontext.posWS, debugGIcontext.posWS + debugGIcontext.normalWS * 0.1, float4(0, 1, 0, 1), 0.01);
    s2h_drawLineWS(debugContext, debugGIcontext.posWS, debugGIcontext.hit.position, float4(0, 1, 0, 1), 0.01);

    s2h_drawLineWS(debugContext, debugGIcontext.hit.position, debugGIcontext.hit.position + debugGIcontext.hit.normal * 2, float4(1, 0, 0, 1), 0.01);

    float4 debugNdc = mul(perScene.camera.viewProjection, float4(debugContext.ro + debugContext.rd * debugContext.depth, 1.0));
    perMaterial.outDebugTex[2 * dispatchThreadID.xy] = float4(debugContext.dstColor.xyz, debugNdc.z / debugNdc.w);
    perMaterial.outDebugTex[2 * dispatchThreadID.xy + uint2(1,0)] = float4(debugContext.dstColor.xyz, debugNdc.z / debugNdc.w);
    perMaterial.outDebugTex[2 * dispatchThreadID.xy + uint2(0,1)] = float4(debugContext.dstColor.xyz, debugNdc.z / debugNdc.w);
    perMaterial.outDebugTex[2 * dispatchThreadID.xy + uint2(1,1)] = float4(debugContext.dstColor.xyz, debugNdc.z / debugNdc.w);
#endif

    perMaterial.outSH0Tex[context.tileCoord] = context.sh0;
    perMaterial.outSH1Tex[context.tileCoord] = context.sh1;
    perMaterial.outSH2Tex[context.tileCoord] = context.sh2;
    perMaterial.outAccumTex[context.tileCoord] = context.accumCount;
}
