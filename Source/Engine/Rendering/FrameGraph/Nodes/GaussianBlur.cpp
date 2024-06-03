#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include <random>

namespace Rendering::FrameGraph
{
class GaussianBlurNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(GaussianBlurNode)
    {
        SetCustomName("Gaussian Blur");
        shader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/PostProcess/GaussianBlur.shad"
            );
        hShader = shader->GetShaderProgram({"_Horizontal"});
        vShader = shader->GetShaderProgram({"_Vertical"});

        AddConfig<ConfigurableType::Bool>("enable", true);

        input.target = AddInputProperty("target", PropertyType::Attachment);
        input.source = AddInputProperty("source", PropertyType::Attachment);
        output.color = AddOutputProperty("color", PropertyType::Attachment);

        Gfx::RG::SubpassAttachment c[] = {{
            0,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store,
        }};
        hPass = Gfx::RG::RenderPass(1, 1);
        hPass.SetSubpass(0, c);
        hPass.SetName("GaussianBlurH");

        vPass = Gfx::RG::RenderPass(1, 1);
        vPass.SetSubpass(0, c);
        vPass.SetName("GaussianBlurV");

        paramsBuffer = GetGfxDriver()->CreateBuffer(
            Gfx::Buffer::CreateInfo{Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform, sizeof(GaussianBlur)}
        );
    }

    void Compile() override
    {
        config.enable = GetConfigurablePtr<bool>("enable");
        Gfx::ClearValue v{0, 0, 0, 0};
        clears = {v};
    }

    void Execute(Gfx::CommandBuffer& cmd, FrameData& renderingData) override
    {
        if (*config.enable)
        {
            auto inputAttachment = input.target->GetValue<AttachmentProperty>();

            GaussianBlur newParam = {glm::vec4{
                1.0f / inputAttachment.desc.GetWidth(),
                1.0f / inputAttachment.desc.GetHeight(),
                inputAttachment.desc.GetWidth(),
                inputAttachment.desc.GetHeight()
            }};
            if (newParam != params)
            {
                params = newParam;
                GetGfxDriver()->UploadBuffer(*paramsBuffer, (uint8_t*)&params, sizeof(GaussianBlur));
            }

            cmd.SetBuffer("GaussianBlur", *paramsBuffer);
            cmd.AllocateAttachment(tmpRT, inputAttachment.desc);
            hPass.SetAttachment(0, tmpRT);
            cmd.UpdateViewportAndScissor(inputAttachment.desc.GetWidth(), inputAttachment.desc.GetHeight());
            cmd.BeginRenderPass(hPass, clears);
            cmd.SetTexture(sourceHandle, input.source->GetValue<AttachmentProperty>().id);
#if ENGINE_DEV_BUILD
            hShader = shader->GetShaderProgram({"_Horizontal"});
            vShader = shader->GetShaderProgram({"_Vertical"});
#endif
            cmd.BindShaderProgram(hShader, hShader->GetDefaultShaderConfig());
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();

            vPass.SetAttachment(0, inputAttachment.id);
            cmd.BeginRenderPass(vPass, clears);
            cmd.SetTexture(sourceHandle, tmpRT);
            cmd.BindShaderProgram(vShader, vShader->GetDefaultShaderConfig());
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }

        output.color->SetValue(input.target->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    Gfx::ShaderProgram* hShader;
    Gfx::ShaderProgram* vShader;
    Gfx::RG::ImageIdentifier tmpRT;
    std::unique_ptr<Gfx::Buffer> paramsBuffer;

    std::vector<Gfx::ClearValue> clears;
    Gfx::RG::RenderPass hPass;
    Gfx::RG::RenderPass vPass;
    Texture* noiseTex;
    Gfx::ShaderBindingHandle sourceHandle = Gfx::ShaderBindingHandle("source");

    struct GaussianBlur
    {
        bool operator==(const GaussianBlur& other) const = default;
        glm::vec4 sourceTexelSize;
    } params;

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

}; // namespace Rendering::FrameGraph
DEFINE_FRAME_GRAPH_NODE(GaussianBlurNode, "80E6C82C-9FD2-442D-9694-8DCD259A9A4D");

} // namespace Rendering::FrameGraph
