#include "../../NodeBlueprint.hpp"
#include "Core/EngineInternalResources.hpp"
#include "Rendering/GlobalIllumination/LightFieldProbes/Probe.hpp"

namespace Rendering::FrameGraph
{
// generate a octaheral map of a probe
class GBufferGenerationNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(GBufferGenerationNode)
    {
        input.drawList = AddInputProperty("drawList", PropertyType::DrawListPointer);
        input.target = AddInputProperty("target", PropertyType::Attachment);

        gbufferPass = Gfx::RG::RenderPass(1, 3);
        Gfx::RG::SubpassAttachment albedo{0};
        Gfx::RG::SubpassAttachment normal{1};
        Gfx::RG::SubpassAttachment depth{2};
        Gfx::RG::SubpassAttachment subpassAttachments[] = {albedo, normal};
    }

    void Compile() override
    {
        drawList = input.drawList->GetValue<DrawList*>();

        gbufferMaterial = Material();
    }

    void Execute(RenderingContext& context, RenderingData& renderingData) override
    {
        if (probes == nullptr)
            return;

        auto& probes = *this->probes;

        for (auto& probe : probes)
        {
            auto& cmd = *renderingData.cmd;
            for (int face = 0; face < 6; ++face)
            {
                SetupCameraViewMatrix(face);

                // set scissor and viewport
                Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight)}};
                cmd.SetScissor(0, 1, &scissor);
                Gfx::Viewport viewport{0, 0, static_cast<float>(rtWidth), static_cast<float>(rtHeight), 0, 1};
                cmd.SetViewport(viewport);

                // clear albedo to black
                // masks to 1 (mainly for ao)
                Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {1, 0}};
                gbufferPass.SetAttachment(0, albedoID);
                gbufferPass.SetAttachment(1, normalID);
                gbufferPass.SetAttachment(2, depthID);
                cmd.BeginRenderPass(gbufferPass, clears);

                if (drawList)
                {
                    // draw opaque objects
                    for (int i = 0; i < drawList->alphaTestIndex; ++i)
                    {
                        auto& draw = drawList->at(i);
                        auto shaderProgram = draw.material->GetShaderProgram("GBuffer");
                        if (shaderProgram)
                        {
                            cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                            cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                            cmd.BindResource(2, draw.shaderResource);
                            cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                            cmd.SetPushConstant(shaderProgram, (void*)&draw.pushConstant);
                            cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                        }
                    }

                    // draw alpha tested objects
                    Shader::EnableFeature("_AlphaTest");
                    for (int i = drawList->alphaTestIndex; i < drawList->transparentIndex; ++i)
                    {
                        auto& draw = drawList->at(i);
                        auto shaderProgram = draw.material->GetShaderProgram("GBuffer");
                        if (shaderProgram)
                        {
                            cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                            cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                            cmd.BindResource(2, draw.shaderResource);
                            cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
                            cmd.SetPushConstant(shaderProgram, (void*)&draw.pushConstant);
                            cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
                        }
                    }
                    Shader::DisableFeature("_AlphaTest");
                }
                cmd.EndRenderPass();
            }
        }
    }

    void SetupCameraViewMatrix(Gfx::CommandBuffer& cmd, LFP::Probe& probe, int face) {}

private:
    std::vector<LFP::Probe>* probes = nullptr;

    struct
    {
        PropertyHandle target;
        PropertyHandle drawList;
        PropertyHandle source;
    } input;

    struct
    {
        PropertyHandle target;
    } output;

    DrawList* drawList;
    Gfx::RG::RenderPass gbufferPass;
    const int rtWidth = 128;
    const int rtHeight = 128;
    Gfx::RG::ImageIdentifier albedoID;
    Gfx::RG::ImageIdentifier normalID;
    Gfx::RG::ImageIdentifier depthID;

    Material gbufferMaterial;

}; // namespace Rendering::FrameGraph
DEFINE_FRAME_GRAPH_NODE(GBufferGenerationNode, "CD20270C-B4E4-4289-8EE3-2FC95682AC13");

} // namespace Rendering::FrameGraph
