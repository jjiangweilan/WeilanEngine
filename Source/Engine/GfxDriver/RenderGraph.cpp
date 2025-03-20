#include "RenderGraph.hpp"
#include "GfxDriver/RenderGraph.hpp"

Gfx::RG::ImageIdentifier Gfx::RG::ImageIdentifier::CreateEmpty()
{
    Gfx::RG::ImageIdentifier id;
    id.image = nullptr;
    id.type = Type::None;
    id.rtHandle = UUID::GetEmptyUUID();
    return id;
}
const Gfx::RG::ImageIdentifier& Gfx::RG::ImageIdentifier::GetEmpty()
{
    static ImageIdentifier empty = CreateEmpty();
    return empty;
}

bool Gfx::RG::RenderPass::IsValidForRendering() const
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
