#include "MainRenderer.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Texture.hpp"
#include "Core/Time.hpp"

namespace Rendering
{

MainRenderer::MainRenderer()
{
    CreateCameraImages();
}

void MainRenderer::Setup(Camera& camera, Gfx::CommandBuffer& cmd)
{
    auto scene = camera.GetScene();
    if (scene == nullptr)
        return;

    SetupFrame(camera, cmd);
}

void MainRenderer::SetupFrame(Camera& camera, Gfx::CommandBuffer& cmd)
{
    auto scene = camera.GetScene();

    renderingData.mainCamera = &camera;
    renderingData.sceneInfo = &sceneInfo;

    auto sceneEnvironment = scene->GetRenderingScene().GetSceneEnvironment();

    auto diffuseCube = sceneEnvironment ? sceneEnvironment->GetDiffuseCube() : nullptr;
    auto specularCube = sceneEnvironment ? sceneEnvironment->GetSpecularCube() : nullptr;
    cmd.SetBuffer("SceneInfo", *sceneInfoBuffer);
    if (diffuseCube)
        cmd.SetTexture("diffuseCube", *diffuseCube->GetGfxImage());
    if (specularCube)
        cmd.SetTexture("specularCube", *specularCube->GetGfxImage());

    renderingData.terrain = scene->GetRenderingScene().GetTerrain();
    GameObject* camGo = camera.GetGameObject();

    glm::mat4 viewMatrix = camera.GetViewMatrix();
    glm::mat4 projectionMatrix = camera.GetProjectionMatrix();
    glm::mat4 vp = projectionMatrix * viewMatrix;
    glm::vec4 viewPos = glm::vec4(camGo->GetPosition(), 1);
    sceneInfo.projection = projectionMatrix;
    sceneInfo.viewProjection = vp;
    sceneInfo.viewPos = viewPos;
    sceneInfo.view = viewMatrix;
    sceneInfo.shadowMapSize = {
        1024,
        1024,
        1.0 / 1024.0f,
        1.0 / 1024.0f,
    };
    sceneInfo.invProjection = glm::inverse(projectionMatrix);
    sceneInfo.invNDCToWorld = glm::inverse(viewMatrix) * glm::inverse(projectionMatrix);
    sceneInfo.cameraZBufferParams = glm::vec4(
        camera.GetNear(),
        camera.GetFar(),
        (camera.GetNear() - camera.GetFar()) / (camera.GetNear() * camera.GetFar()),
        1.0f / camera.GetNear()
    );
    sceneInfo.cameraFrustum = glm::vec4(
        -camera.GetProjectionRight(),
        camera.GetProjectionRight(),
        -camera.GetProjectionTop(),
        camera.GetProjectionTop()
    );
    sceneInfo.screenSize = glm::vec4(
        renderingData.screenSize.x,
        renderingData.screenSize.y,
        1.0f / renderingData.screenSize.x,
        1.0f / renderingData.screenSize.y
    );
    ProcessLights(*scene);

    size_t copySize = sceneInfoBuffer->GetSize();
    Gfx::BufferCopyRegion regions[] = {{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = copySize,
    }};

    memcpy(stagingBuffer->GetCPUVisibleAddress(), &sceneInfo, sizeof(sceneInfo));
    cmd.CopyBuffer(stagingBuffer, sceneInfoBuffer, regions);

    shaderGlobal.time = Time::TimeSinceLaunch();
    GetGfxDriver()->UploadBuffer(*shaderGlobalBuffer, (uint8_t*)&shaderGlobal, sizeof(ShaderGlobal));
    cmd.SetBuffer("Global", *shaderGlobalBuffer);
}

void MainRenderer::Execute(Gfx::CommandBuffer& cmd) {}

void MainRenderer::CreateCameraImages()
{
    mainColor = Gfx::RG::ImageIdentifier("_MainColor");
    mainColor = Gfx::RG::ImageIdentifier("_MainDepth");
}

void MainRenderer::ProcessLights(Scene& gameScene)
{
    auto lights = gameScene.GetActiveLights();

    Light* shadowLight = nullptr;
    for (int i = 0; i < lights.size(); ++i)
    {
        sceneInfo.lights[i].ambientScale = lights[i]->GetAmbientScale();
        sceneInfo.lights[i].lightColor = glm::vec4(lights[i]->GetLightColor(), 1.0);
        sceneInfo.lights[i].intensity = lights[i]->GetIntensity();
        auto model = lights[i]->GetGameObject()->GetModelMatrix();
        glm::vec4 position{};
        switch (lights[i]->GetLightType())
        {
            case LightType::Directional:
                {
                    glm::vec3 pos = -glm::normalize(glm::vec3(model[2]));
                    sceneInfo.lights[i].position = {pos, 0};

                    if (shadowLight == nullptr || shadowLight->GetIntensity() < lights[i]->GetIntensity())
                    {
                        shadowLight = lights[i];
                    }
                    break;
                }
            case LightType::Point:
                {
                    glm::vec3 pos = model[3];
                    sceneInfo.lights[i].position = {pos, 1};
                    sceneInfo.lights[i].pointLightTerm1 = lights[i]->GetPointLightLinear();
                    sceneInfo.lights[i].pointLightTerm2 = lights[i]->GetPointLightQuadratic();
                    break;
                }
        }
    }

    sceneInfo.lightCount = lights.size();
    if (shadowLight)
    {
        sceneInfo.worldToShadow =
            shadowLight->WorldToShadowMatrix(gameScene.GetMainCamera()->GetGameObject()->GetPosition());
    }
}
} // namespace Rendering
