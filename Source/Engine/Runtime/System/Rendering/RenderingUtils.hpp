#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/GPUParameter.hpp"

class Camera;
namespace Rendering
{
class RenderingUtils
{
public:
    static void DynamicScaleImageDescription(Gfx::RenderImageDescriptor& desc, const glm::vec2& scale);

    // dispatch all registered graphics command in Graphics.DrawXXX API
    // this should be called inside a RenderPass
    static void DrawGraphics(Gfx::CommandBuffer& cmd);
    static GPUParameter::Camera CreateCameraGPUParameter(Camera& camera, float2 screenSize);
    static GPUParameter::Camera CreateCameraGPUParameter(
        float3 position,
        const float4x4& viewMatrix,
        const float4x4& projectionMatrix,
        float near,
        float far,
        float top,
        float right,
        float2 screenSize
    );
};

} // namespace Rendering
