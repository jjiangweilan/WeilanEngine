#include "../NodeBlueprint.hpp"

namespace Rendering::FrameGraph
{
class CopyNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(CopyNode)
    {
        input.target = AddInputProperty("target", PropertyType::Attachment, 1);
        input.source = AddInputProperty("source", PropertyType::Attachment, 2);

        output.target = AddOutputProperty("target", PropertyType::Attachment, 3);

        SetCustomName("Copy");
    }

    void Compile() override {}

    void Execute(RenderingContext& context, RenderingData& renderingData) override
    {
        auto& cmd = *renderingData.cmd;
        cmd.Blit(input.source->GetValue<AttachmentProperty>().id, input.target->GetValue<AttachmentProperty>().id);
        output.target->SetValue(input.target->GetValue<AttachmentProperty>());
    }

private:
    struct
    {
        PropertyHandle target;
        PropertyHandle source;
    } input;

    struct
    {
        PropertyHandle target;
    } output;

}; // namespace Rendering::FrameGraph
DEFINE_FRAME_GRAPH_NODE(CopyNode, "996C6B2B-56D4-4DA2-B3E6-A73A0DDAF6B4");

} // namespace Rendering::FrameGraph
