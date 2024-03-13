#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"

namespace FrameGraph
{
class SSRNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(SSRNode)
    {
        AddConfig<ConfigurableType::Float>("iteration", 32.0f);
        AddConfig<ConfigurableType::Float>("thickness", 0.5f);

        input.target = AddInputProperty("target", PropertyType::Attachment);
        input.source = AddInputProperty("source color", PropertyType::Attachment);
        input.normal = AddInputProperty("normal", PropertyType::Attachment);
        input.mask = AddInputProperty("mask", PropertyType::Attachment);
        input.hiz = AddInputProperty("hiz", PropertyType::GraphFlow);

        output.color = AddOutputProperty("output", PropertyType::Attachment);

        ssrPass = Gfx::RG::RenderPass::SingleColor("ssr pass");

        ssrShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/PostProcess/SSR.shad");

        ssrResource = GetGfxDriver()->CreateShaderResource();
        ssrBuffer = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            sizeof(SSR),
            false,
            "SSR parameters",
            false});

        ssrResource->SetBuffer("SSR", ssrBuffer.get());
    }

    void Compile() override
    {
        thickness = GetConfigurablePtr<float>("thickness");
        iteration = GetConfigurablePtr<float>("iteration");
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        auto source = input.source->GetValue<AttachmentProperty>();
        SSR ssr{
            .targetRTSize = glm::vec4(
                source.desc.GetWidth(),
                source.desc.GetHeight(),
                1.0f / source.desc.GetWidth(),
                1.0f / source.desc.GetHeight()
            ),
            .iteration = *iteration,
            .thickness = *thickness,
            .downsampleScale = 1.0f,
        };

        GetGfxDriver()->UploadBuffer(*ssrBuffer, (uint8_t*)&ssr, sizeof(SSR));

        Gfx::ClearValue clears[] = {{0, 0, 0, 0}};
        ssrPass.SetAttachment(0, input.target->GetValue<AttachmentProperty>().id);
        cmd.BeginRenderPass(ssrPass, clears);
        cmd.SetTexture("sourceTex", source.id);
        cmd.SetTexture("normalTex", input.normal->GetValue<AttachmentProperty>().id);
        cmd.SetTexture("maskTex", input.mask->GetValue<AttachmentProperty>().id);
        cmd.BindResource(2, ssrResource.get());
        // hiz is set by hiz node
        cmd.BindShaderProgram(ssrShader->GetDefaultShaderProgram(), ssrShader->GetDefaultShaderConfig());
        cmd.Draw(6, 1, 0, 0);
        cmd.EndRenderPass();
    }

private:
    Gfx::RG::RenderPass ssrPass;
    Shader* ssrShader;

    float* thickness;
    float* iteration;

    struct SSR
    {
        glm::vec4 targetRTSize;
        float iteration;
        float thickness;
        float downsampleScale;
    };
    struct
    {
        PropertyHandle target;
        PropertyHandle source;
        PropertyHandle normal;
        PropertyHandle mask;
        PropertyHandle hiz;
    } input;

    struct
    {
        PropertyHandle color;
    } output;

    std::unique_ptr<Gfx::ShaderResource> ssrResource;
    std::unique_ptr<Gfx::Buffer> ssrBuffer;
};

DEFINE_FRAME_GRAPH_NODE(SSRNode, "131CD668-C0ED-4995-AB0F-444D33A0618B");
} // namespace FrameGraph
