#include "RenderGraph.hpp"
#include "GfxDriver/RenderGraph.hpp"

Gfx::ImageIdentifier Gfx::ImageIdentifier::CreateEmpty()
{
    Gfx::ImageIdentifier id;
    id.image = nullptr;
    id.type = Type::None;
    id.rtHandle = UUID::GetEmptyUUID();
    return id;
}
const Gfx::ImageIdentifier& Gfx::ImageIdentifier::GetEmpty()
{
    static ImageIdentifier empty = CreateEmpty();
    return empty;
}

bool Gfx::RenderPass::IsValidForRendering() const
{
    for (auto& subpass : subpasses)
    {
        // requires color but it's not set
        for (auto& c : subpass.colors)
        {
            if (attachments[c.attachmentIndex] == ImageIdentifier::GetEmpty())
                return false;
        }

        // requires depth but it's not set
        if (subpass.depth.attachmentIndex != -1 &&
            attachments[subpass.depth.attachmentIndex] == ImageIdentifier::GetEmpty())
            return false;

        // no attachmet set
        if (subpass.colors.empty() && subpass.depth.attachmentIndex == -1)
            return false;
    }

    return true;
}
