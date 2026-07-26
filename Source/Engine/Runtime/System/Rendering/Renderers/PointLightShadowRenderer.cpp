#include "PointLightShadowRenderer.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Rendering
{

static const glm::vec3 faceDirections[6] = {
    {1, 0, 0},  // +X
    {-1, 0, 0}, // -X
    {0, 1, 0},  // +Y
    {0, -1, 0}, // -Y
    {0, 0, 1},  // +Z
    {0, 0, -1}, // -Z
};

static const glm::vec3 faceUps[6] = {
    {0, -1, 0}, // +X
    {0, -1, 0}, // -X
    {0, 0, 1},  // +Y
    {0, 0, -1}, // -Y
    {0, -1, 0}, // +Z
    {0, -1, 0}, // -Z
};

void PointLightShadowRenderer::Init()
{
    pass = Gfx::RenderPass(1, 1);
    pass.SetSubpass(
        0,
        {},
        Gfx::SubpassAttachment{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    );
    pass.SetName("Point Light Shadow Map pass");

    shadowMapShader = ShaderLibrary::GetShader(Shaders::PointLightShadowMapObject);
    shadowMapShaderGPUDriven = ShaderLibrary::GetShader(Shaders::PointLightShadowMapObject, {"_GPUDriven"});
    shadowMapShaderTerrain = ShaderLibrary::GetShader(
        Shaders::PointLightShadowMapObject,
        {"_GPUDriven", "_Terrain"}
    );
    shadowMapShaderTree = ShaderLibrary::GetShader(
        Shaders::PointLightShadowMapObject,
        {"_GPUDriven", "_TreeWind"}
    );
    terrainShader = ShaderLibrary::GetShader(Shaders::Terrain);

    CreateCubemapResources();
}

void PointLightShadowRenderer::CreateCubemapResources()
{
    Gfx::ImageDescription desc(
        shadowMapSize,
        shadowMapSize,
        1,
        Gfx::GfxFormat::D32_SFloat,
        Gfx::MultiSampling::Sample_Count_1,
        1,
        true // isCubemap = true (6 layers)
    );

    shadowCubemap = GetGfxDriver()->CreateImage(
        desc,
        Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::Texture
    );
    shadowCubemap->SetName("Point Light Shadow Cubemap");

    // Per-face rendering views (2D, single array layer)
    for (int i = 0; i < 6; ++i)
    {
        Gfx::ImageView::CreateInfo createInfo{
            shadowCubemap.get(),
            Gfx::ImageViewType::Image_2D,
            Gfx::ImageSubresourceRange{
                Gfx::ImageAspect::Depth,
                0,
                1,
                (uint32_t)i,
                1
            }
        };
        faceViews[i] = GetGfxDriver()->CreateImageView(createInfo);
    }

    GetGfxDriver()->InitGfxImage(*shadowCubemap, float4(1, 1, 1, 1));

    // Cubemap sampling view (all 6 faces, type Cubemap)
    cubemapSamplingView = &shadowCubemap->GetImageView(
        Gfx::ImageViewOption{0, 1, 0, 6, Gfx::ImageAspect::Depth, Gfx::ImageViewOption::Type::Cubemap}
    );

    // Per-face UBO buffers
    for (int i = 0; i < 6; ++i)
    {
        faceBuffers[i] = GetGfxDriver()->CreateBuffer(
            sizeof(GPUParameter::PointLightShadowPass),
            Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst
        );
    }

    initialized = true;
}

glm::mat4 PointLightShadowRenderer::GetFaceViewProjection(
    int faceIndex, const glm::vec3& lightPos, float nearPlane, float farPlane
)
{
    glm::mat4 view = glm::lookAtRH(
        lightPos,
        lightPos + faceDirections[faceIndex],
        faceUps[faceIndex]
    );
    // perspectiveRH_ZO: depth [0,1], standard order (near→0, far→1)
    glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
    return proj * view;
}

void PointLightShadowRenderer::Setup(Light& light, RenderingData& renderingData)
{
    currentFarPlane = light.GetRange();
    currentDepthBias = 0.005f;
    glm::mat4 model = light.GetGameObject()->GetWorldMatrix();
    currentLightPosition = glm::vec3(model[3]);

    float nearPlane = glm::max(0.05f * currentFarPlane, 0.01f);

    for (int i = 0; i < 6; ++i)
    {
        glm::mat4 worldToShadow = GetFaceViewProjection(i, currentLightPosition, nearPlane, currentFarPlane);

        GPUParameter::PointLightShadowPass faceParam;
        faceParam.worldToShadow = worldToShadow;
        faceParam.lightPosition = currentLightPosition;
        faceParam.farPlane = currentFarPlane;

        GetGfxDriver()->UploadBuffer(
            *faceBuffers[i],
            (uint8_t*)&faceParam,
            sizeof(GPUParameter::PointLightShadowPass)
        );
    }
}

void PointLightShadowRenderer::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    if (!initialized)
        return;

    DrawList shadowDrawList;
    shadowDrawList.AddShadowCasters(renderingData.scene->GetRenderingScene().GetMeshRenderers());
    shadowDrawList.Lock();

    cmd.BeginLabel("Point Light Shadow Map", {0.2f, 0.6f, 0.2f, 1.0f});

    auto program = shadowMapShader->GetShaderProgram();
    auto programGPUDriven = shadowMapShaderGPUDriven->GetShaderProgram();
    auto programTerrain = shadowMapShaderTerrain->GetShaderProgram();
    auto programTree = shadowMapShaderTree->GetShaderProgram();
    auto terrainProgram = terrainShader->GetShaderProgram();

    Gfx::Viewport viewport = {0, 0, (float)shadowMapSize, (float)shadowMapSize, 0, 1};
    Rect2D scissor{{0, 0}, {shadowMapSize, shadowMapSize}};

    Gfx::ClearValue clearValues[] = {{0.0f, 0}}; // reverzed-Z: clear to 0 (near, no occluder)

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        pass.SetAttachment(0, *faceViews[faceIndex]);

        cmd.BeginRenderPass(pass, clearValues);
        cmd.SetViewport(viewport);
        cmd.SetScissor(0, 1, &scissor);
        cmd.SetDepthBiasEnable(false);

        std::vector<Gfx::DynamicBinding> bindings = {Gfx::DynamicBinding("shadowPass", *faceBuffers[faceIndex])};
        cmd.BindResource(1, bindings);

        for (auto& drawIdx : shadowDrawList.GetSortedIndices())
        {
            auto& draw = shadowDrawList[drawIdx];
            if (draw.skinned)
                continue;

            auto* materialProgram = draw.material != nullptr ? draw.material->GetShaderProgram() : nullptr;
            bool isTree = materialProgram != nullptr &&
                          materialProgram->GetName() == ShaderLibrary::GetShaderName(Shaders::TreeSceneLit);
            auto* programUsed = materialProgram == terrainProgram
                                    ? programTerrain
                                    : isTree ? programTree : program;
            auto ps = draw.GetPushConstant();
            cmd.SetPushConstant(programUsed, (void*)&ps);
            cmd.BindShaderProgram(programUsed, programUsed->GetDefaultShaderConfig());
            cmd.Draw(draw.indexCount, 1, 0, 0);
        }

        if (renderingData.gpuDrivenIndirectBuffer && renderingData.gpuDrivenIndirectDrawCount > 0)
        {
            for (auto& group : *renderingData.gpuObjectShaderGroups)
            {
                if (!group.castsShadows)
                    continue;

                bool isTree = group.shaderProgram->GetName() ==
                              ShaderLibrary::GetShaderName(Shaders::TreeSceneLit);
                auto* programUsed = group.shaderProgram == terrainProgram
                                        ? programTerrain
                                        : isTree ? programTree : programGPUDriven;
                cmd.BindShaderProgram(programUsed, *group.pipelineConfig);
                cmd.BindIndexBuffer(GPUDrivenManager::Instance().GetGlobalBuffer(), 0, Gfx::IndexBufferType::UInt32);

                struct PushConstant
                {
                    uint32_t firstGpuObjectOffset;
                } pconst = {group.firstDrawIndex};
                cmd.SetPushConstant(programUsed, &pconst);

                cmd.DrawIndexedIndirect(
                    renderingData.gpuDrivenIndirectBuffer,
                    group.firstDrawIndex * 20,
                    group.drawCount,
                    20
                );
            }
        }

        cmd.EndRenderPass();
    }

    cmd.EndLabel();
}

} // namespace Rendering
