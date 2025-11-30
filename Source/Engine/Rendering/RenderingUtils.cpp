#include "RenderingUtils.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "Graphics.hpp"

namespace Rendering
{
void RenderingUtils::DynamicScaleImageDescription(Gfx::RenderImageDescriptor& desc, const glm::vec2& scale)
{
    const auto& baseSize = glm::vec2{desc.GetWidth(), desc.GetHeight()};
    if (scale.x == 0)
    {
        desc.SetWidth(baseSize.x);
    }
    else if (scale.x < 0)
    {
        desc.SetWidth(baseSize.x * -scale.x);
    }
    else
        desc.SetWidth(scale.x);
    if (scale.y == 0)
    {
        desc.SetHeight(baseSize.y);
    }
    else if (scale.y < 0)
    {
        desc.SetHeight(baseSize.y * -scale.y);
    }
    else
        desc.SetHeight(scale.y);
}

void RenderingUtils::DrawGraphics(Gfx::CommandBuffer& cmd)
{
    Graphics::GetSingleton().DispatchDraws(cmd);
}

GPUParameter::Camera RenderingUtils::CreateCameraGPUParameter(
    float3 position,
    const float4x4& viewMatrix,
    const float4x4& projectionMatrix,
    float near,
    float far,
    float top,
    float right,
    float2 screenSize
)
{
    float4x4 vp = projectionMatrix * viewMatrix;
    glm::float4 viewPos = glm::float4(position, 1);

    GPUParameter::Camera cameraParam{};

    // update camera parameters
    cameraParam.position = viewPos;
    cameraParam.cameraZBufferParams = glm::vec4(near, far, (far - near) / (near * far), 1.0f / far);
    cameraParam.cameraFrustum = glm::vec4(-right, right, -top, top);
    cameraParam.view = viewMatrix;
    cameraParam.projection = projectionMatrix;
    cameraParam.viewProjection = vp;
    cameraParam.invProjection = glm::inverse(projectionMatrix);
    cameraParam.invNDCToWorld = glm::inverse(viewMatrix) * cameraParam.invProjection;
    cameraParam.screenSize = glm::vec4(screenSize.x, screenSize.y, 1.0f / screenSize.x, 1.0f / screenSize.y);

    return cameraParam;
}

GPUParameter::Camera RenderingUtils::CreateCameraGPUParameter(Camera& camera, float2 screenSize)
{
    auto camGo = camera.GetGameObject();
    return (CreateCameraGPUParameter(
        camGo->GetPosition(),
        camera.GetViewMatrix(),
        camera.GetAndUpdateProjectionMatrix(screenSize.x / screenSize.y),
        camera.GetNear(),
        camera.GetFar(),
        camera.GetProjectionTop(),
        camera.GetProjectionRight(),
        screenSize
    ));
}
} // namespace Rendering
