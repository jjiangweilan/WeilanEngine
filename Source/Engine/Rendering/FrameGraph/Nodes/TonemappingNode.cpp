
#include "AssetDatabase/AssetDatabase.hpp"
#include "../NodeBlueprint.hpp"
#include "Rendering/Shader.hpp"

namespace Rendering::FrameGraph
{
class TonemappingNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(TonemappingNode)
    {
        input.source = AddInputProperty("source", PropertyType::Attachment);
        input.targetColor = AddInputProperty("target color", PropertyType::Attachment);
        output.color = AddOutputProperty("color", PropertyType::Attachment);

        shader = (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/PostProcess/ACESToneMapping.shad");
    }

public:
    void Compile() override
    {
        clears = {{0, 0, 0, 0}};
    }

    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override
    {
        auto& cmd = *renderingData.cmd;
        auto id = input.source->GetValue<AttachmentProperty>().id;
        auto targetColor = input.targetColor->GetValue<AttachmentProperty>().id;
        if (shader != nullptr)
        {
            mainPass.SetAttachment(0, targetColor);
            cmd.BeginRenderPass(mainPass, clears);
            cmd.SetTexture(sourceHandle, id);
            cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }

        output.color->SetValue(input.targetColor->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    std::vector<Gfx::ClearValue> clears;
    const DrawList* drawList;
    Gfx::RG::RenderPass mainPass = Gfx::RG::RenderPass::SingleColor("tone mapping pass");
    Gfx::ShaderBindingHandle sourceHandle = Gfx::ShaderBindingHandle("source");

    struct
    {
        PropertyHandle source;
        PropertyHandle targetColor;
    } input;

    struct
    {
        PropertyHandle color;
    } output;
}; // namespace Rendering::FrameGraph

DEFINE_FRAME_GRAPH_NODE(TonemappingNode, "00226DA1-0333-438B-B4A2-108514A202A8");
} // namespace Rendering::FrameGraph
