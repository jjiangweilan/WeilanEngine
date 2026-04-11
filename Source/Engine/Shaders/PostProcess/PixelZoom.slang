#if CONFIG
name: PostProcess/PixelZoom
#endif

#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;

struct PushConstant
{
    float2 mousePos;    // mouse position in pixels
    float2 screenSize;  // screen dimensions in pixels
    float zoomFactor;
    float zoomWindowSize; // half-size of zoom window in pixels
}
[vk::push_constant]
PushConstant p;

struct PerMaterial
{
    Texture2D input;
    SamplerState sampler_point_clamp;
}

[Material]
ParameterBlock<PerMaterial> perMaterial;

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float2 fragPos = input.uv * p.screenSize; // pixel coords

    float2 delta = fragPos - p.mousePos;
    float halfWin = p.zoomWindowSize;

    bool insideWindow = abs(delta.x) <= halfWin && abs(delta.y) <= halfWin;

    if (insideWindow)
    {
        // Border: 2 pixels wide
        bool isBorder = abs(delta.x) > halfWin - 2.0 || abs(delta.y) > halfWin - 2.0;
        if (isBorder)
            return float4(1.0, 1.0, 0.0, 1.0); // yellow border

        // Map fragment within zoom window to zoomed UV
        float2 zoomedOffset = delta / p.zoomFactor;
        float2 zoomedUV = (p.mousePos + zoomedOffset) / p.screenSize;
        return perMaterial.input.SampleLevel(perMaterial.sampler_point_clamp, zoomedUV, 0);
    }
    else
    {
        return perMaterial.input.SampleLevel(perMaterial.sampler_point_clamp, input.uv, 0);
    }
}
