#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "GfxDriver/Image.hpp"

namespace Rendering::FrameGraph
{
class ShadowMapNode : public Node
{
    DECLARE_OBJECT();

public:
    ShadowMapNode()
    {
        DefineNode();
    };
    ShadowMapNode(FGID id) : Node("Shadow Map", id)
    {
        DefineNode();
    }

    void Compile() override
    {
        shadowMapSize = GetConfigurableVal<glm::vec2>("shadow map size");
        shadowmapShader = GetConfigurableVal<Shader*>("shadow map shader");
        shadowMapClears = {{1.0f, 0}};
        shadowPass.SetSubpass(
            0,
            {},
            Gfx::RG::SubpassAttachment{0, Gfx::AttachmentLoadOperation::Clear, Gfx::AttachmentStoreOperation::Store}
        );
    }

    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override
    {
        auto& cmd = *renderingData.cmd;
        drawList = input.drawList->GetValue<DrawList*>();

        if (shadowmapShader)
        {
            cmd.SetViewport(
                {.x = 0, .y = 0, .width = shadowMapSize.x, .height = shadowMapSize.y, .minDepth = 0, .maxDepth = 1}
            );
            Rect2D rect = {{0, 0}, {(uint32_t)shadowMapSize.x, (uint32_t)shadowMapSize.y}};
            cmd.SetScissor(0, 1, &rect);
            shadowDescription.SetWidth(shadowMapSize.x);
            shadowDescription.SetHeight(shadowMapSize.y);
            shadowDescription.SetFormat(Gfx::ImageFormat::D32_SFloat);
            cmd.AllocateAttachment(shadowMapId, shadowDescription);
            shadowPass.SetAttachment(0, shadowMapId);
            cmd.BeginRenderPass(shadowPass, shadowMapClears);
            auto program = shadowmapShader->GetShaderProgram(0);

            for (auto& draw : *drawList)
            {
                cmd.BindShaderProgram(program, program->GetDefaultShaderConfig());
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.SetPushConstant(program, (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }

            cmd.EndRenderPass();

            cmd.SetTexture("shadowMap", shadowMapId);
        }

        output.shadowMap->SetValue(AttachmentProperty{shadowMapId, shadowDescription});
    }

private:
    Gfx::RG::RenderPass shadowPass = Gfx::RG::RenderPass(1, 1);
    Gfx::RG::ImageIdentifier shadowMapId;
    Gfx::RG::ImageDescription shadowDescription;

    std::vector<Gfx::ClearValue> shadowMapClears;
    Shader* shadowmapShader;
    glm::vec2 shadowMapSize;

    const DrawList* drawList;

    struct
    {
        PropertyHandle drawList;
    } input;

    struct
    {
        PropertyHandle shadowMap;
    } output;
    void DefineNode()
    {
        output.shadowMap = AddOutputProperty("shadow map", PropertyType::Attachment);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer);

        AddConfig<ConfigurableType::Vec2>("shadow map size", glm::vec2{1024, 1024});
        AddConfig<ConfigurableType::ObjectPtr>("shadow map shader", nullptr);
    }
    static char _reg;
};

char ShadowMapNode::_reg = NodeBlueprintRegisteration::Register<ShadowMapNode>("Shadow Map");
DEFINE_OBJECT(ShadowMapNode, "A486879A-5EA3-456A-ACF0-ED3E536826A0");
} // namespace Rendering::FrameGraph
