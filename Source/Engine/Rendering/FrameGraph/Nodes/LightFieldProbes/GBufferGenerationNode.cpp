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

        Gfx::RG::SubpassAttachment albedo{0};
        Gfx::RG::SubpassAttachment normal{1};
        Gfx::RG::SubpassAttachment depth{2};
        Gfx::RG::SubpassAttachment subpassAttachments[] = {albedo, normal};
    }

    void Compile() override
    {
        /*drawList = input.drawList->GetValue<DrawList*>();

        gbufferMaterial =
            std::make_unique<Material>(EngineInternalResources::GetLightProbeFieldGBufferGenerationShader());

        albedoCubemap = GetGfxDriver()->CreateImage(
            Gfx::ImageDescription{rtWidth, rtHeight, Gfx::ImageFormat::R8G8B8A8_SRGB},
            Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
        );
        normalCubemap = GetGfxDriver()->CreateImage(
            Gfx::ImageDescription{rtWidth, rtHeight, Gfx::ImageFormat::A2B10G10R10_UNorm},
            Gfx::ImageUsage::Texture | Gfx::ImageUsage::ColorAttachment
        );
        depthCubeMap = GetGfxDriver()->CreateImage(
            Gfx::ImageDescription{rtWidth, rtHeight, Gfx::ImageFormat::D32_SFloat},
            Gfx::ImageUsage::Texture | Gfx::ImageUsage::DepthStencilAttachment
        );
        for (int i = 0; i < 6; ++i)
        {
            cubemapRenderPasses[i].Init(albedoCubemap.get(), normalCubemap.get(), depthCubeMap.get(), i);
        }*/
    }

    void Execute(RenderingContext& context, RenderingData& renderingData) override {}

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
    Gfx::RG::RenderPass projectPass = Gfx::RG::RenderPass::Default("LPF project");
    Gfx::RG::ImageIdentifier albedoID;
    Gfx::RG::ImageIdentifier normalID;
    Gfx::RG::ImageIdentifier depthID;

    std::unique_ptr<Material> gbufferMaterial;

}; // namespace Rendering::FrameGraph
DEFINE_FRAME_GRAPH_NODE(GBufferGenerationNode, "CD20270C-B4E4-4289-8EE3-2FC95682AC13");

} // namespace Rendering::FrameGraph
