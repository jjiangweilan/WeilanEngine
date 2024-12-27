#include "RenderGraph.hpp"

Gfx::RG::ImageIdentifier Gfx::RG::ImageIdentifier::CreateEmpty()
{
    Gfx::RG::ImageIdentifier id;
    id.image = nullptr;
    id.type = Type::None;
    return id;
}
const Gfx::RG::ImageIdentifier& Gfx::RG::ImageIdentifier::GetEmpty()
{
    static ImageIdentifier empty = CreateEmpty();
    return empty;
}
