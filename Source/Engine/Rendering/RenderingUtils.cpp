#include "RenderingUtils.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "Graphics.hpp"

namespace Rendering
{
void RenderingUtils::DynamicScaleImageDescription(Gfx::RG::ImageDescription& desc, const glm::vec2& scale)
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
GPUParameter::Camera RenderingUtils::CreateCameraGPUParameter(Camera& camera, float2 screenSize)
{
    auto camGo = camera.GetGameObject();

    float4x4 viewMatrix = camera.GetViewMatrix();
    float4x4 projectionMatrix = camera.GetAndUpdateProjectionMatrix(screenSize.x / screenSize.y);
    float4x4 vp = projectionMatrix * viewMatrix;
    glm::float4 viewPos = glm::float4(camGo->GetPosition(), 1);

    GPUParameter::Camera cameraParam{};

    // update camera parameters
    cameraParam.position = viewPos;
    cameraParam.cameraZBufferParams = glm::vec4(
        camera.GetNear(),
        camera.GetFar(),
        (camera.GetNear() - camera.GetFar()) / (camera.GetNear() * camera.GetFar()),
        1.0f / camera.GetNear()
    );
    cameraParam.cameraFrustum = glm::vec4(
        -camera.GetProjectionRight(),
        camera.GetProjectionRight(),
        -camera.GetProjectionTop(),
        camera.GetProjectionTop()
    );
    cameraParam.view = viewMatrix;
    cameraParam.projection = projectionMatrix;
    cameraParam.viewProjection = vp;
    cameraParam.invProjection = glm::inverse(projectionMatrix);
    cameraParam.invNDCToWorld = glm::inverse(viewMatrix) * cameraParam.invProjection;
    cameraParam.screenSize = glm::vec4(screenSize.x, screenSize.y, 1.0f / screenSize.x, 1.0f / screenSize.y);

    return cameraParam;
}
} // namespace Rendering
