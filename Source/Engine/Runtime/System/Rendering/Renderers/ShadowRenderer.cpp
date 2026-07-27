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
    shadowMapShaderTerrain = ShaderLibrary::GetShader(Shaders::ShadowMapObject, {"_GPUDriven", "_Terrain"});
    shadowMapShaderTree = ShaderLibrary::GetShader(Shaders::ShadowMapObject, {"_GPUDriven", "_TreeWind"});
    terrainShader = ShaderLibrary::GetShader(Shaders::Terrain);

    ResetShadowmap(1.0f, 4);
}

void ShadowRenderer::Setup(Light& light, RenderingData& renderingData)
{
    bool reconfigShadowMap = false;
    int cascadeCount = light.IsCascadeShadowEnabled() ? light.GetCascadeCount() : 1;
    int shadowMapSizeScale = cascadeCount;
    if (currentShadowMapInfo.cascadeCount != cascadeCount)
    {
        currentShadowMapInfo.cascadeCount = cascadeCount;
        reconfigShadowMap = true;
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

    // Calculate the cascade extent in view space so camera translation and rotation cannot resize it.
    Frustum frustum(projection, Frustum::CornersOnly{});
    auto corners = frustum.corners;
    float3 frustumCenter = float3(0.0f);
    for (const auto& corner : corners)
    {
        frustumCenter += corner;
    }
    frustumCenter /= float(corners.size());

    float cascadeRadius = 0.0f;
    for (const auto& corner : corners)
    {
        cascadeRadius = glm::max(cascadeRadius, glm::distance(frustumCenter, corner));
    }
    constexpr float frustumPadding = 2.5f;
    cascadeRadius += frustumPadding;

    auto invView = glm::inverse(view);
    frustumCenter = invView * float4(frustumCenter, 1.0f);
    for (auto& corner : corners)
    {
        corner = invView * float4(corner, 1.0f);
    }

    // Use rotation only so scale on the light or its parents cannot distort the shadow view.
    auto lightMatrix = glm::mat4_cast(light.GetGameObject()->GetRotation());
    lightMatrix[2] = -lightMatrix[2];

    auto worldToLightRotation = glm::inverse(lightMatrix);
    float3 frustumCenterLS = worldToLightRotation * float4(frustumCenter, 1.0f);
    float worldUnitsPerTexel = (cascadeRadius * 2.0f) / shadowMapTexelSize.z;
    frustumCenterLS.x = glm::round(frustumCenterLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    frustumCenterLS.y = glm::round(frustumCenterLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;
    lightMatrix[3] = lightMatrix * float4(frustumCenterLS, 1.0f);

    float4x4 worldToLight = glm::inverse(lightMatrix);
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (auto& corner : corners)
    {
        corner = worldToLight * float4(corner, 1.0f);
        minZ = glm::min(minZ, corner.z);
        maxZ = glm::max(maxZ, corner.z);
    }

    constexpr float casterDepthMargin = 300.0f;
    minZ -= casterDepthMargin + frustumPadding;
    maxZ += frustumPadding;

    // Swap the Y bounds to retain the engine's Vulkan projection orientation and use reversed Z.
    glm::mat4 proj = glm::orthoLH_ZO(
        -cascadeRadius,
        cascadeRadius,
        cascadeRadius,
        -cascadeRadius,
        maxZ,
        minZ
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

    int shadowCascadeCount = std::clamp(
        int(renderingData.gpuMainLightShadow->shadowCascadeCount),
        1,
        GPUParameter::MAX_SHADOW_MAP_CASCADE_COUNT
    );
    bool shadowFrustumCull = renderingData.renderPipelineSettings->shadowFrustumCull;
    float3 shadowSortPosition =
        renderingData.mainCamera->GetGameObject()->GetPosition() +
        mainLight->GetLightDirection() * mainLight->GetMainLightNearPlane();

    DrawList sharedShadowDrawList;
    std::vector<DrawList> cascadeShadowDrawLists;
    if (shadowFrustumCull)
    {
        cascadeShadowDrawLists.resize(shadowCascadeCount);
        for (int cascadeIndex = 0; cascadeIndex < shadowCascadeCount; ++cascadeIndex)
        {
            Frustum frustum(renderingData.gpuMainLightShadow->worldToShadow[cascadeIndex]);
            auto renderers = renderingData.scene->GetBVHScene().QueryMeshRenderersInFrustum(frustum);
            auto& shadowDrawList = cascadeShadowDrawLists[cascadeIndex];
            shadowDrawList.AddShadowCasters(renderers);
            shadowDrawList.Lock();
            shadowDrawList.SortByDistance(shadowSortPosition);
        }
    }
    else
    {
        sharedShadowDrawList.AddShadowCasters(renderingData.scene->GetRenderingScene().GetMeshRenderers());
        sharedShadowDrawList.Lock();
        sharedShadowDrawList.SortByDistance(shadowSortPosition);
    }

    cmd.BeginLabel("Shadow Map", {0.11, 0.376, 0.729, 1.0});
    {
        if (updateMainLightShadow)
        {
            auto mainLight = renderingData.GetMainLight();

            if (mainLight)
            {
                Gfx::ClearValue shadowMapClears[] = {{0.0f, 0}};
                cmd.BeginRenderPass(pass, shadowMapClears);
                for (int cascadeIndex = 0; cascadeIndex < shadowCascadeCount; ++cascadeIndex)
                {
                    auto& shadowDrawList = shadowFrustumCull
                                               ? cascadeShadowDrawLists[cascadeIndex]
                                               : sharedShadowDrawList;
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
                    auto programTerrain = shadowMapShaderTerrain->GetShaderProgram();
                    auto terrainProgram = terrainShader->GetShaderProgram();
                    auto programTree = shadowMapShaderTree->GetShaderProgram();

                    for (auto& drawIdx : shadowDrawList.GetSortedIndices())
                    {
                        auto& draw = shadowDrawList[drawIdx];
                        auto materialProgram = draw.material != nullptr ? draw.material->GetShaderProgram() : nullptr;
                        bool isTree = materialProgram != nullptr &&
                                      materialProgram->GetName() == ShaderLibrary::GetShaderName(Shaders::TreeSceneLit);
                        auto programUsed = materialProgram == terrainProgram
                                               ? programTerrain
                                               : isTree ? programTree : program;
                        auto ps = draw.GetPushConstant();
                        [[unlikely]]
                        if (draw.skinned)
                        {
                            programUsed = programSkinned;
                            cmd.SetPushConstant(programUsed, (void*)&ps);
                            if (draw.objectResource)
                                cmd.BindResource(2, draw.objectResource);
                        }
                        else
                        {
                            cmd.SetPushConstant(programUsed, (void*)&ps);
                        }
                        cmd.BindShaderProgram(programUsed, programUsed->GetDefaultPipelineConfig());

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
                            auto programGPUDriven = group.shaderProgram == terrainProgram
                                                        ? programTerrain
                                                        : isTree
                                                              ? programTree
                                                              : shadowMapShaderGPUDriven->GetShaderProgram();
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
