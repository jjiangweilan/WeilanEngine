#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"

namespace FrameGraph
{
class FullScreenPassNode : public Node
{
    DECLARE_OBJECT();

public:
    FullScreenPassNode()
    {
        DefineNode();
    };
    FullScreenPassNode(FGID id) : Node("Full Screen Pass", id)
    {
        DefineNode();
    }

    void Compile() override
    {
        shader = GetConfigurableVal<Shader*>("shader");
        passResource = GetGfxDriver()->CreateShaderResource();

        ClearConfigs();
        AddConfig<ConfigurableType::ObjectPtr>("shader", shader);

        if (shader)
        {
            for (auto& b : shader->GetDefaultShaderProgram()->GetShaderInfo().bindings)
            {
                if (b.second.type == Gfx::ShaderInfo::BindingType::UBO)
                {
                    for (auto& m : b.second.binding.ubo.data.members)
                    {
                        if (m.second.data->type == Gfx::ShaderInfo::ShaderDataType::Float)
                        {
                            AddConfig<ConfigurableType::Float>(m.first.data(), 0.0f);
                        }
                    }
                }
            }
        }

        Gfx::ClearValue v;
        v.color.float32[0] = 0;
        v.color.float32[1] = 0;
        v.color.float32[2] = 0;
        v.color.float32[3] = 0;

        clears = {v};
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        auto id = input.source->GetValue<AttachmentProperty>().id;
        auto targetColor = input.targetColor->GetValue<AttachmentProperty>().id;
        if (shader != nullptr)
        {
            mainPass.SetAttachment(0, targetColor);
            cmd.BeginRenderPass(mainPass, clears);
            cmd.SetTexture(sourceHandle, id);
            cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
            cmd.BindResource(1, passResource.get());
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }

        output.color->SetValue(input.targetColor->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    std::vector<Gfx::ClearValue> clears;
    const DrawList* drawList;
    std::unique_ptr<Gfx::ShaderResource> passResource;
    Gfx::RG::RenderPass mainPass = Gfx::RG::RenderPass::SingleColor("full screen pass");
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

    void DefineNode()
    {
        AddConfig<ConfigurableType::ObjectPtr>("shader", nullptr);
        AddConfig<ConfigurableType::Vec2Int>("size", glm::ivec2{512.0f, 512.0f});

        input.source = AddInputProperty("source", PropertyType::Attachment);
        input.targetColor = AddInputProperty("target color", PropertyType::Attachment);
        output.color = AddOutputProperty("color", PropertyType::Attachment);
    }
    static char _reg;
}; // namespace FrameGraph

char FullScreenPassNode::_reg = NodeBlueprintRegisteration::Register<FullScreenPassNode>("Full Screen Pass");

DEFINE_OBJECT(FullScreenPassNode, "F52A89D5-D830-4DD2-84DF-6A82A5F9F4CD");
} // namespace FrameGraph
