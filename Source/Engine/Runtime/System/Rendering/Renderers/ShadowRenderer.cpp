#include "ShadowRenderer.hpp"
#include "Engine/MiddleLayer/EngineDebug.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"

namespace Rendering
{
void ShadowRenderer::Init()
{
    pass = Gfx::RenderPass(1, 1);
    pass.SetSubpass(
        0,
        {},
        Gfx::SubpassAttachment{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
    );
    pass.SetName("ShadowMap pass");
    shadowMapShader = ShaderLibrary::GetShader(Shaders::ShadowMapObject);
    shadowMapShaderSkinned = ShaderLibrary::GetShader(Shaders::ShadowMapObjectSkinned);
    shadowMapShaderGPUDriven = ShaderLibrary::GetShader(Shaders::ShadowMapObject, {"_GPUDriven"});

    ResetShadowmap(1.0f, 4);
}

void ShadowRenderer::Setup(Light& light, RenderingData& renderingData)
{
    bool reconfigShadowMap = false;
    int cascadeCount = light.GetCascadeCount();
    int shadowMapSizeScale = 1.0f;
    if (currentShadowMapInfo.cascadeCount != cascadeCount)
    {
        currentShadowMapInfo.cascadeCount = light.GetCascadeCount();
        reconfigShadowMap = true;
    }

    if (light.IsCascadeShadowEnabled() && cascadeCount != 0)
    {
        shadowMapSizeScale *= cascadeCount;
    }

    if (reconfigShadowMap || shadowMap == nullptr)
    {
        ResetShadowmap(shadowMapSizeScale, cascadeCount);
    }
}

void ShadowRenderer::ResetShadowmap(float shadowMapSizeScale, int cascadeCount)
{
    cascadeBuffers.clear();
    shadowDescription = Gfx::ImageDescription(shadowMapTexelSize.z * shadowMapSizeScale, shadowMapTexelSize.w, Gfx::GfxFormat::D32_SFloat);

    shadowMap = GetGfxDriver()->CreateImage(
        shadowDescription,
        Gfx::ImageUsage::DepthStencilAttachment | Gfx::ImageUsage::Texture
    );
    shadowMapId = *shadowMap;

    pass.SetAttachment(0, shadowMapId);

    for (int i = 0; i < cascadeCount; ++i)
    {
        auto cascadeBuffer = GetGfxDriver()->CreateBuffer(sizeof(GPUParameter::ShadowPass), Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst);
        cascadeBuffers.push_back(std::move(cascadeBuffer));

        GPUParameter::ShadowPass shadowPass{(float)i};
        GetGfxDriver()->UploadBuffer(
            *cascadeBuffers[i],
            (uint8_t*)&shadowPass,
            sizeof(GPUParameter::ShadowPass)
        );
    }
}

void ShadowRenderer::SetSettings(ShadowRendererSettigns settings) {}

float4x4 ShadowRenderer::GetWorldToShadowMatrix(Light& light, RenderingData& renderingData, float shadowDistance)
{
    auto view = renderingData.gpuCamera->view;
    auto projection = renderingData.mainCamera->CalculateProjectionMatrixWithOverride(
        shadowDistance,
        renderingData.screenAspect
    );
    auto vp = projection * view;
    Frustum frustum(vp, Frustum::CornersOnly{});
    auto corners = frustum.corners;

    auto lightMatrix = light.GetGameObject()->GetWorldMatrix();
    lightMatrix[0] = float4(glm::normalize(float3(lightMatrix[0])), 0.0);
    lightMatrix[1] = float4(glm::normalize(float3(lightMatrix[1])), 0.0);
    lightMatrix[2] = float4(glm::normalize(float3(lightMatrix[2])), 0.0);
    lightMatrix[2] = -lightMatrix[2];
    lightMatrix[3] = float4(float3(renderingData.gpuCamera->position), 1); // no translation

    float4x4 worldToLight = glm::inverse(lightMatrix);
    for (int i = 0; i < corners.size(); i++)
    {
        corners[i] = worldToLight * glm::vec4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
    }

    AABB shadowFrustumAABB;
    shadowFrustumAABB.min = float3(std::numeric_limits<float>::max());
    shadowFrustumAABB.max = float3(std::numeric_limits<float>::lowest());

    for (int i = 0; i < corners.size(); i++)
    {
        shadowFrustumAABB.min = glm::min(shadowFrustumAABB.min, corners[i]);
        shadowFrustumAABB.max = glm::max(shadowFrustumAABB.max, corners[i]);
    }

    // not the best solution, but it prevents shaow pixel swimming when the camera is moving
    {
        shadowFrustumAABB.min.x = glm::round(shadowFrustumAABB.min.x);
        shadowFrustumAABB.min.y = glm::round(shadowFrustumAABB.min.y);
        shadowFrustumAABB.max.x = glm::round(shadowFrustumAABB.max.x);
        shadowFrustumAABB.max.y = glm::round(shadowFrustumAABB.max.y);
    }

    shadowFrustumAABB.min.z -= 300.0f; // reserve some space for what's behind the camera

    // note: we swap min max of y in this case because the ortho is not symmetric in origin, simplely negate the y axis
    // won't work
    glm::mat4 proj = glm::orthoLH_ZO(
        shadowFrustumAABB.min.x,
        shadowFrustumAABB.max.x,
        shadowFrustumAABB.max.y,
        shadowFrustumAABB.min.y,
        shadowFrustumAABB.max.z,
        shadowFrustumAABB.min.z
    );
    auto ret = proj * worldToLight;

    if (EngineDebugVars::ShadowFrustum())
        Graphics::DrawFrustum(ret);
    return ret;
}

void ShadowRenderer::Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    auto mainLight = renderingData.GetMainLight();
    if (!mainLight)
    {
        return;
    }

    DrawList shadowDrawList;
    std::vector<MeshRenderer*> renderers{};
    if (renderingData.renderPipelineSettings->shadowFrustumCull)
    {
        Frustum frustum(renderingData.gpuMainLightShadow->worldToShadow[0]);
        auto renderers = renderingData.scene->GetRenderingScene().QueryRendererInFrustum(frustum);
        shadowDrawList.Add(renderers);
    }
    else
    {
        shadowDrawList.Add(renderingData.scene->GetRenderingScene().GetMeshRenderers());
    }

    shadowDrawList.Lock();
    shadowDrawList.SortByDistance(
        renderingData.mainCamera->GetGameObject()->GetPosition() +
        mainLight->GetLightDirection() * mainLight->GetMainLightNearPlane()
    );

    cmd.BeginLabel("Shadow Map", {0.11, 0.376, 0.729, 1.0});
    {
        if (updateMainLightShadow)
        {
            auto mainLight = renderingData.GetMainLight();

            if (mainLight)
            {
                auto shadowCascadeCount = mainLight->GetCascadeCount();

                Gfx::ClearValue shadowMapClears[] = {{0.0f, 0}};
                cmd.BeginRenderPass(pass, shadowMapClears);
                for (int cascadeIndex = 0; cascadeIndex < shadowCascadeCount; ++cascadeIndex)
                {
                    Rect2D scissor{{(int)shadowMapTexelSize.z * cascadeIndex, 0}, {(uint32_t)shadowMapTexelSize.z, (uint32_t)shadowMapTexelSize.w}};
                    Gfx::Viewport viewport = Gfx::Viewport{cascadeIndex * shadowMapTexelSize.z, 0, shadowMapTexelSize.z, shadowMapTexelSize.w, 0, 1};

                    cmd.SetDepthBiasEnable(true);
                    cmd.SetDepthBias(mainLight->depthBias, 0, mainLight->depthSlopeBias);
                    cmd.SetViewport(viewport);
                    cmd.SetScissor(0, 1, &scissor);

                    std::vector<Gfx::DynamicBinding> bindings = {Gfx::DynamicBinding("shadowPass", *cascadeBuffers[cascadeIndex])};

                    cmd.BindResource(1, bindings);

                    auto program = shadowMapShader->GetShaderProgram();
                    auto programSkinned = shadowMapShaderSkinned->GetShaderProgram();

                    for (auto& drawIdx : shadowDrawList.GetSortedIndices())
                    {
                        auto& draw = shadowDrawList[drawIdx];
                        auto programUsed = program;
                        [[unlikely]]
                        if (draw.skinned)
                        {
                            programUsed = programSkinned;
                            if (draw.objectResource)
                                cmd.BindResource(2, draw.objectResource);
                        }
                        else
                        {
                            auto ps = draw.GetPushConstant();
                            cmd.SetPushConstant(programUsed, (void*)&ps);
                        }
                        cmd.BindShaderProgram(programUsed, programUsed->GetDefaultShaderConfig());

                        cmd.Draw(draw.indexCount, 1, 0, 0);
                    }

                    if (renderingData.gpuDrivenIndirectBuffer && renderingData.gpuDrivenIndirectDrawCount > 0)
                    {
                        for (auto& group : *renderingData.gpuObjectShaderGroups)
                        {
                            auto programGPUDriven = shadowMapShaderGPUDriven->GetShaderProgram();
                            cmd.BindShaderProgram(programGPUDriven, *group.pipelineConfig);

                            // Bind global index buffer for GPU-driven rendering
                            cmd.BindIndexBuffer(GPUDrivenManager::Instance().GetGlobalBuffer(), 0, Gfx::IndexBufferType::UInt32);

                            // Set push constant for GPU-driven draw
                            struct PushConstant
                            {
                                uint32_t firstGpuObjectOffset;
                            } pconst = {group.firstDrawIndex}; // Currently we only have one group for all GPU objects
                            cmd.SetPushConstant(programGPUDriven, &pconst);

                            cmd.DrawIndexedIndirect(
                                renderingData.gpuDrivenIndirectBuffer,
                                group.firstDrawIndex * 20,
                                group.drawCount,
                                20 // sizeof(DrawIndexedIndirectCommand)
                            );
                        }
                    }
                }
                cmd.EndRenderPass();
                cmd.SetDepthBias(0, 0, 0);
            }
        }
    }
    cmd.EndLabel();
}
} // namespace Rendering
  //
