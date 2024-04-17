#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include <random>

namespace FrameGraph
{
class GaussianBlurNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(GaussianBlurNode)
    {
        SetCustomName("Gaussian Blur");
        shader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/PostProcess/GaussianBlur.shad"
            );

        AddConfig<ConfigurableType::Bool>("enable", true);

        input.target = AddInputProperty("target", PropertyType::Attachment);
        input.source = AddInputProperty("source", PropertyType::Attachment);
        output.color = AddOutputProperty("color", PropertyType::Attachment);

        Gfx::RG::SubpassAttachment c[] = {{
            0,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store,
        }};
        mainPass = Gfx::RG::RenderPass(1, 1);
        mainPass.SetSubpass(0, c);
        mainPass.SetName("GaussianBlur");
    }

    void Compile() override
    {
        config.enable = GetConfigurablePtr<bool>("enable");
        Gfx::ClearValue v{0, 0, 0, 0};
        clears = {v};
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        if (*config.enable)
        {
            auto inputAttachment = input.target->GetValue<AttachmentProperty>();
            mainPass.SetAttachment(0, inputAttachment.id);
            cmd.UpdateViewportAndScissor(inputAttachment.desc.GetWidth(), inputAttachment.desc.GetHeight());
            cmd.BeginRenderPass(mainPass, clears);
            cmd.SetTexture(sourceHandle, input.source->GetValue<AttachmentProperty>().id);
            cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
            cmd.Draw(6, 1, 0, 0);

            cmd.EndRenderPass();
        }

        output.color->SetValue(input.target->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    std::vector<Gfx::ClearValue> clears;
    Gfx::RG::RenderPass mainPass;
    Texture* noiseTex;
    Gfx::ShaderBindingHandle sourceHandle = Gfx::ShaderBindingHandle("source");

    struct
    {
        bool* enable;
    } config;

    struct
    {
        PropertyHandle target;
        PropertyHandle source;
    } input;

    struct
    {
        PropertyHandle color;
    } output;

}; // namespace FrameGraph
DEFINE_FRAME_GRAPH_NODE(GaussianBlurNode, "80E6C82C-9FD2-442D-9694-8DCD259A9A4D");

} // namespace FrameGraph
