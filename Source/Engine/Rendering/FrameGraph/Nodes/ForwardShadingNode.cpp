#include "../NodeBlueprint.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include "Core/Scene/RenderingScene.hpp"
#include "Rendering/Renderers/GrassSurfaceRenderer.hpp"
#include "Rendering/RenderingUtils.hpp"
#include "Rendering/Shader.hpp"
#include <spdlog/spdlog.h>

namespace Rendering::FrameGraph
{
class ForwardShadingNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(ForwardShadingNode)
    {
        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.shadowMap = AddInputProperty("shadow map", PropertyType::Attachment);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer);

        output.color = AddOutputProperty("color", PropertyType::Attachment);
        output.depth = AddOutputProperty("depth", PropertyType::Attachment);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("skybox", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("cloud noise material", nullptr);
        clearValues.resize(2);
    }

    void Compile() override
    {
        clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");
        volumetricFogShader = static_cast<Shader*>(
            AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/VolumetricFog.shad")
        );

        drawList = input.drawList->GetValue<DrawList*>();
    }

    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override
    {
        auto& cmd = *renderingData.cmd;

        for (auto grassSurface : renderingData.renderingScene->GetGrassSurface())
        {
            grassSurfaceRenderer.DispatchCompute(*grassSurface, cmd);
        }

        auto desc = input.color->GetValue<AttachmentProperty>().desc;

        uint32_t width = desc.GetWidth();
        uint32_t height = desc.GetHeight();
        cmd.SetViewport({.x = 0, .y = 0, .width = (float)width, .height = (float)height, .minDepth = 0, .maxDepth = 1});
        Rect2D rect = {{0, 0}, {width, height}};
        clearValues[0] = *clearValuesVal;
        clearValues[0].color = {{(*clearValuesVal)[0], (*clearValuesVal)[1], (*clearValuesVal)[2], (*clearValuesVal)[3]}
        };
        clearValues[1].depthStencil = {1};

        cmd.SetScissor(0, 1, &rect);
        mainPass.SetAttachment(0, input.color->GetValue<AttachmentProperty>().id);
        mainPass.SetAttachment(1, input.depth->GetValue<AttachmentProperty>().id);
        cmd.BeginRenderPass(mainPass, clearValues);

        for (auto grassSurface : renderingData.renderingScene->GetGrassSurface())
        {
            grassSurfaceRenderer.Draw(*grassSurface, cmd);
        }

        RenderingUtils::DrawGraphics(cmd);
        // draw scene objects
        // for (auto& draw : *drawList)
        // {
        //     cmd.BindShaderProgram(draw.shader->GetShaderProgram(0, 0), *draw.shaderConfig);
        //     cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
        //     cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
        //     cmd.BindResource(2, draw.shaderResource);
        //     cmd.SetPushConstant(draw.shader->GetShaderProgram(0, 0), (void*)&draw.pushConstant);
        //     cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
        // }

        cmd.EndRenderPass();

        output.color->SetValue(input.color->GetValue<AttachmentProperty>());
        output.depth->SetValue(input.depth->GetValue<AttachmentProperty>());
    }

private:
    GrassSurfaceRenderer grassSurfaceRenderer;

    Gfx::RG::RenderPass fogGenerationPass = Gfx::RG::RenderPass::SingleColor();
    Shader* volumetricFogShader;
    Gfx::RG::RenderPass mainPass = Gfx::RG::RenderPass::Default(
        "Forward Shading Pass",
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store,
        Gfx::AttachmentLoadOperation::Load,
        Gfx::AttachmentStoreOperation::Store
    );

    const DrawList* drawList;
    std::vector<Gfx::ClearValue> clearValues;

    glm::vec4* clearValuesVal;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
        PropertyHandle shadowMap;
        PropertyHandle drawList;
    } input;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
    } output;

    uint32_t cloudTexSize = 128;
    uint32_t cloudTex2Size = 64;
}; // namespace Rendering::FrameGraph

DEFINE_FRAME_GRAPH_NODE(ForwardShadingNode, "E6188926-D83E-4B17-9C7C-060A5862BDCA");
} // namespace Rendering::FrameGraph
