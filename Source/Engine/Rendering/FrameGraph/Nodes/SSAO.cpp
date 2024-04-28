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
        noiseTex = (Texture*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Textures/noise.ktx");

        if (aoType == AOType::AlchmeyAO)
        {
            shaderProgram = shader->GetShaderProgram({"_AlchemyAO"});
        }

        input.attachment = AddInputProperty("attachment", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.normal = AddInputProperty("normal", PropertyType::Attachment);

        output.color = AddOutputProperty("attachment", PropertyType::Attachment);

        AddConfig<ConfigurableType::Bool>("enable", true);
        AddConfig<ConfigurableType::Float>("bias", 0.0f);
        AddConfig<ConfigurableType::Float>("range check", 1.f);
        AddConfig<ConfigurableType::Float>("radius", 0.5f);
        AddConfig<ConfigurableType::Float>("stength", 1.0f);
        AddConfig<ConfigurableType::Float>("k", 1.0f);
        AddConfig<ConfigurableType::Float>("self bias", 0.001f);
        AddConfig<ConfigurableType::Float>("theta", 1.0f);
        AddConfig<ConfigurableType::Float>("sample count", 8.f);

        Gfx::RG::SubpassAttachment c[] = {{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store,
        }};
        mainPass = Gfx::RG::RenderPass(1, 1);
        mainPass.SetSubpass(0, c);
        mainPass.SetName("SSAO");

        // setup blur
        blur.shader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/PostProcess/GaussianBlur.shad"
            );
        blur.hShader = blur.shader->GetShaderProgram({"_Horizontal"});
        blur.vShader = blur.shader->GetShaderProgram({"_Vertical"});
        Gfx::RG::SubpassAttachment blurAttachments[] = {{
            0,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store,
        }};
        blur.hPass = Gfx::RG::RenderPass(1, 1);
        blur.hPass.SetSubpass(0, blurAttachments);
        blur.hPass.SetName("GaussianBlurH");

        blur.vPass = Gfx::RG::RenderPass(1, 1);
        blur.vPass.SetSubpass(0, c);
        blur.vPass.SetName("GaussianBlurV");
        blur.paramsBuffer = GetGfxDriver()->CreateBuffer(
            Gfx::Buffer::CreateInfo{Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform, sizeof(GaussianBlur)}
        );
        blur.config = blur.hShader->GetDefaultShaderConfig();
        blur.config.color.blends.push_back(Gfx::ColorBlendAttachmentState{});
        blur.config.color.blends[0].colorWriteMask = Gfx::ColorComponentBit::Component_B_Bit;
        blur.tmpRT = Gfx::RG::ImageIdentifier("SSAO-blur-tmp");
    }

    void Compile() override
    {
        enable = GetConfigurablePtr<bool>("enable");
        bias = GetConfigurablePtr<float>("bias");
        radius = GetConfigurablePtr<float>("radius");
        rangeCheck = GetConfigurablePtr<float>("range check");
        passResource = GetGfxDriver()->CreateShaderResource();
        sigma = GetConfigurablePtr<float>("stength");
        k = GetConfigurablePtr<float>("k");
        theta = GetConfigurablePtr<float>("theta");
        beta = GetConfigurablePtr<float>("self bias");
        sampleCount = GetConfigurablePtr<float>("sample count");

        switch (aoType)
        {
            case AOType::CrysisAO:
                {
                    ssaoBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
                        Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
                        sizeof(RandomSamples),
                        false,
                        "SSAO Buffer",
                        false
                    });

                    ssaoParamBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
                        Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
                        sizeof(CrysisAO),
                        false,
                        "SSAO param Buffer",
                        false
                    });
                    break;
                }

            case AOType::AlchmeyAO:
                {
                    ssaoParamBuf = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
                        Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
                        sizeof(Alchmey),
                        false,
                        "SSAO param Buffer",
                        false
                    });
                    break;
                }
        }

        Gfx::ClearValue v;
        v.color.float32[0] = 0;
        v.color.float32[1] = 0;
        v.color.float32[2] = 0;
        v.color.float32[3] = 0;
        clears = {v};

        if (aoType == AOType::CrysisAO)
        {
            RandomSamples ssao{};
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

            GetGfxDriver()->UploadBuffer(*ssaoBuf, (uint8_t*)&ssao, sizeof(RandomSamples));
            passResource->SetBuffer("SSAO", ssaoBuf.get());
        }
        passResource->SetBuffer("SSAOParam", ssaoParamBuf.get());
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        auto inputAttachment = input.attachment->GetValue<AttachmentProperty>();
        if (*enable)
        {
            auto depth = input.depth->GetValue<AttachmentProperty>().id;
            auto normal = input.normal->GetValue<AttachmentProperty>();

            float width = normal.desc.GetWidth();
            float height = normal.desc.GetHeight();

            if (aoType == AOType::CrysisAO)
            {
                CrysisAO newParam{{1 / width, 1 / height, width, height}, *bias, *radius, *rangeCheck};
                if (crysis != newParam)
                {
                    crysis = newParam;
                    GetGfxDriver()->UploadBuffer(*ssaoParamBuf, (uint8_t*)&crysis, sizeof(CrysisAO));
                }
            }
            else if (aoType == AOType::AlchmeyAO)
            {
                Alchmey newParam{
                    {1 / width, 1 / height, width, height},
                    *sigma,
                    *k,
                    *beta,
                    *theta,
                    *sampleCount,
                    *radius,
                    *rangeCheck
                };
                if (alchmey != newParam)
                {
                    alchmey = newParam;
                    GetGfxDriver()->UploadBuffer(*ssaoParamBuf, (uint8_t*)&alchmey, sizeof(Alchmey));
                }
            }

#if ENGINE_DEV_BUILD
            if (aoType == AOType::AlchmeyAO)
                shaderProgram = shader->GetShaderProgram({"_AlchemyAO"});
#endif

            if (shader != nullptr)
            {
                mainPass.SetAttachment(0, inputAttachment.id);
                cmd.BeginRenderPass(mainPass, clears);
                cmd.BindResource(1, passResource.get());
                cmd.SetTexture("noise", *noiseTex->GetGfxImage());
                cmd.SetTexture(depthHandle, depth);
                cmd.SetTexture(normalHandle, normal.id);
                cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                cmd.Draw(6, 1, 0, 0);
                cmd.EndRenderPass();
            }

            // blur
            GaussianBlur newParam = {glm::vec4{
                1.0f / inputAttachment.desc.GetWidth(),
                1.0f / inputAttachment.desc.GetHeight(),
                inputAttachment.desc.GetWidth(),
                inputAttachment.desc.GetHeight()
            }};
            if (newParam != blurParams)
            {
                blurParams = newParam;
                GetGfxDriver()->UploadBuffer(*blur.paramsBuffer, (uint8_t*)&blurParams, sizeof(GaussianBlur));
            }

            cmd.SetBuffer("GaussianBlur", *blur.paramsBuffer);
            cmd.AllocateAttachment(blur.tmpRT, inputAttachment.desc);
            blur.hPass.SetAttachment(0, blur.tmpRT);
            cmd.UpdateViewportAndScissor(inputAttachment.desc.GetWidth(), inputAttachment.desc.GetHeight());
            cmd.BeginRenderPass(blur.hPass, clears);
            cmd.SetTexture(sourceHandle, inputAttachment.id);
#if ENGINE_DEV_BUILD
            blur.hShader = blur.shader->GetShaderProgram({"_Horizontal"});
            blur.vShader = blur.shader->GetShaderProgram({"_Vertical"});
#endif
            cmd.BindShaderProgram(blur.hShader, blur.config);
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();

            blur.vPass.SetAttachment(0, inputAttachment.id);
            cmd.BeginRenderPass(blur.vPass, clears);
            cmd.SetTexture(sourceHandle, blur.tmpRT);
            cmd.BindShaderProgram(blur.vShader, blur.config);
            cmd.Draw(6, 1, 0, 0);
            cmd.EndRenderPass();
        }

        output.color->SetValue(input.attachment->GetValue<AttachmentProperty>());
    }

private:
    enum class AOType
    {
        CrysisAO,
        AlchmeyAO
    } aoType = AOType::AlchmeyAO;

    Shader* shader;
    Gfx::ShaderProgram* shaderProgram;
    float* bias;
    float* sigma;
    float* k;
    float* beta;
    float* theta;
    float* radius;
    float* rangeCheck;
    float* sampleCount;

    bool* enable;
    std::vector<Gfx::ClearValue> clears;
    std::unique_ptr<Gfx::ShaderResource> passResource;
    std::unique_ptr<Gfx::Buffer> ssaoBuf;
    std::unique_ptr<Gfx::Buffer> ssaoParamBuf;
    Gfx::RG::RenderPass mainPass;
    Gfx::ShaderBindingHandle depthHandle = Gfx::ShaderBindingHandle("depth");
    Gfx::ShaderBindingHandle normalHandle = Gfx::ShaderBindingHandle("normal");
    Gfx::ShaderBindingHandle sourceHandle = Gfx::ShaderBindingHandle("source");
    Texture* noiseTex;

    struct GaussianBlur
    {
        bool operator==(const GaussianBlur& other) const = default;
        glm::vec4 sourceTexelSize;
    } blurParams;

    struct BlurPass
    {
        Shader* shader;
        Gfx::ShaderProgram* hShader;
        Gfx::ShaderProgram* vShader;
        Gfx::RG::ImageIdentifier tmpRT;
        std::unique_ptr<Gfx::Buffer> paramsBuffer;
        Gfx::RG::RenderPass hPass;
        Gfx::RG::RenderPass vPass;
        Gfx::ShaderConfig config;
    } blur;

    struct RandomSamples
    {
        glm::vec4 samples[SSAO_SAMPLE_COUNT];
    };

    struct CrysisAO
    {
        bool operator==(const CrysisAO& other) const = default;

        glm::vec4 sourceTexelSize;
        float bias;
        float radius;
        float rangeCheck;
    } crysis;

    struct Alchmey
    {
        bool operator==(const Alchmey& other) const = default;

        glm::vec4 sourceTexelSize;
        float sigma;
        float k;
        float beta;
        float theta;

        float sampleCount;
        float radius;
        float rangeCheck;
    } alchmey;

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
