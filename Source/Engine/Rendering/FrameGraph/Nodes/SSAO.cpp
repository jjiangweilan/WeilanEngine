#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include <random>

namespace FrameGraph
{
#define SSAO_SAMPLE_COUNT 64
class SSAONode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(SSAONode)
    {
        SetCustomName("SSAO z channel");
        shader = (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/PostProcess/SSAO.shad");

        input.attachment = AddInputProperty("attachment", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.normal = AddInputProperty("normal", PropertyType::Attachment);

        output.color = AddOutputProperty("attachment", PropertyType::Attachment);

        AddConfig<ConfigurableType::Float>("bias", 0.0f);
        AddConfig<ConfigurableType::Float>("radius", 0.3f);
        enable = AddConfig<ConfigurableType::Bool>("enable", true);

        Gfx::RG::SubpassAttachment c[] = {{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store,
        }};
        mainPass = Gfx::RG::RenderPass(1, 1);
        mainPass.SetSubpass(0, c);
        mainPass.SetName("SSAO");
    }

    void Compile() override
    {
        bias = GetConfigurablePtr<float>("bias");
        radius = GetConfigurablePtr<float>("radius");

        passResource = GetGfxDriver()->CreateShaderResource();
        ssaoBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            sizeof(SSAO),
            false,
            "SSAO Buffer",
            false
        });

        ssaoParamBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            sizeof(SSAOParam),
            false,
            "SSAO param Buffer",
            false
        });

        Gfx::ClearValue v;
        v.color.float32[0] = 0;
        v.color.float32[1] = 0;
        v.color.float32[2] = 0;
        v.color.float32[3] = 0;
        clears = {v};

        SSAO ssao{};
        // from LearnOpenGL
        std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
        std::default_random_engine generator;
        for (unsigned int i = 0; i < SSAO_SAMPLE_COUNT; ++i)
        {
            glm::vec4 sample(
                randomFloats(generator) * 2.0 - 1.0,
                randomFloats(generator) * 2.0 - 1.0,
                randomFloats(generator),
                1.0
            );
            sample = glm::normalize(sample);
            sample *= randomFloats(generator);

            float scale = (float)i / 64.0;
            scale = 0.1f + 0.9 * scale * scale; // lerp
            sample *= scale;

            ssao.samples[i] = glm::vec4(glm::normalize(glm::vec3(sample)), 1.0);
        }

        GetGfxDriver()->UploadBuffer(*ssaoBuf, (uint8_t*)&ssao, sizeof(SSAO));
        passResource->SetBuffer("SSAO", ssaoBuf.get());
        passResource->SetBuffer("SSAOParam", ssaoParamBuf.get());
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        if (*enable)
        {
            SSAOParam newParam{*bias, *radius};
            if (ssaoParam != newParam)
            {
                ssaoParam = newParam;
                GetGfxDriver()->UploadBuffer(*ssaoParamBuf, (uint8_t*)&ssaoParam, sizeof(SSAOParam));
            }
            auto depth = input.depth->GetValue<AttachmentProperty>().id;
            auto normal = input.normal->GetValue<AttachmentProperty>().id;
            auto attachment = input.attachment->GetValue<AttachmentProperty>().id;
            if (shader != nullptr)
            {
                mainPass.SetAttachment(0, attachment);
                cmd.BeginRenderPass(mainPass, clears);
                cmd.BindShaderProgram(shader->GetDefaultShaderProgram(), shader->GetDefaultShaderConfig());
                cmd.BindResource(1, passResource.get());
                cmd.SetTexture(depthHandle, depth);
                cmd.SetTexture(normalHandle, normal);
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();
            }
        }

        output.color->SetValue(input.attachment->GetValue<AttachmentProperty>());
    }

private:
    Shader* shader;
    float* bias;
    float* radius;
    bool* enable;
    std::vector<Gfx::ClearValue> clears;
    std::unique_ptr<Gfx::ShaderResource> passResource;
    std::unique_ptr<Gfx::Buffer> ssaoBuf;
    std::unique_ptr<Gfx::Buffer> ssaoParamBuf;
    Gfx::RG::RenderPass mainPass;
    Gfx::ShaderBindingHandle depthHandle = Gfx::ShaderBindingHandle("depth");
    Gfx::ShaderBindingHandle normalHandle = Gfx::ShaderBindingHandle("normal");

    struct SSAO
    {
        glm::vec4 samples[SSAO_SAMPLE_COUNT];
    };

    struct SSAOParam
    {
        bool operator==(const SSAOParam& other) const = default;

        float bias;
        float radius;
    } ssaoParam;

    struct
    {
        PropertyHandle attachment;
        PropertyHandle depth;
        PropertyHandle normal;
    } input;

    struct
    {
        PropertyHandle color;
    } output;

}; // namespace FrameGraph
DEFINE_FRAME_GRAPH_NODE(SSAONode, "D80FC5C3-E17B-45BD-90C8-C6384D6362A1");

} // namespace FrameGraph
