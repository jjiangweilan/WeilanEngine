#include "VertexAttributes.hpp"
#include "Libs/Hash.hpp"

size_t VertexAttributes::GetAttributeHash() const
{
    if (hash == 0)
    {
        Rehash();
    }

    return hash;
}

void VertexAttributes::Rehash() const
{
    hash = 0;
    for (auto& a : attributes)
    {
        HashCombine(hash, a.semanticName);
        HashCombine(hash, a.semanticIndex);
        HashCombine(hash, a.size);
    }
}

VertexAttributeSemantics MapVertexAttributeSemantics(std::string_view name)
{
    if (name == "POSITION")
        return VertexAttributeSemantics::Position;
    if (name == "NORMAL")
        return VertexAttributeSemantics::Normal;
    if (name == "TANGENT")
        return VertexAttributeSemantics::Tangent;
    if (name == "TEXCOORD")
        return VertexAttributeSemantics::Texcoord;
    if (name == "COLOR")
        return VertexAttributeSemantics::Color;
    if (name == "BONE")
        return VertexAttributeSemantics::Bone;

    return VertexAttributeSemantics::Position;
}
