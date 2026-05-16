#if CONFIG
name: SSILSmoothNormal
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;

struct SSILSmoothNormalParameter
{
    float4 rtSize; // (width, height, 1/width, 1/height)

    Texture2D depthTex;

    [format("r32ui")]
    RWTexture2D<uint> outNormalTex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<SSILSmoothNormalParameter> perMaterial;

[numthreads(8, 8, 1)]
[shader("compute")]
void Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= perMaterial.rtSize.x || pixel.y >= perMaterial.rtSize.y)
        return;

    float2 uv = (float2(pixel) + 0.5) * perMaterial.rtSize.zw;
    float depth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;

    if (depth <= 0.0000001)
    {
        perMaterial.outNormalTex[pixel] = 0;
        return;
    }

    float3 centerPos;
    float3 normalWS = NormalReconstructionHigh2(
        perMaterial.depthTex,
        perMaterial.sampler_point_clamp,
        uv,
        perMaterial.rtSize.zw,
        perScene.camera.cameraZBufferParams,
        perScene.camera.invNDCToWorld,
        centerPos
    );

    perMaterial.outNormalTex[pixel] = PackNormalOctahedral16(normalize(normalWS));
}