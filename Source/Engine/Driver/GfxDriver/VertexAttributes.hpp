#pragma once
#include <string>
#include "Library/DynamicArray.hpp"

struct VertexBinding
{
    std::size_t byteOffset;
    std::size_t byteSize;
    std::string name;
};

enum class VertexAttributeSemantics
{
    Position,
    Normal,
    Tangent,
    Texcoord,
    Color,
    Bone, // index = int(bone * 0.1), weight = bone - index * 10
};

VertexAttributeSemantics MapVertexAttributeSemantics(std::string_view name);

class VertexAttributes
{
public:
    struct Attribute
    {
        std::string name;
        VertexAttributeSemantics semanticName;
        int semanticIndex;
        int size;
    };

    bool HasAttribute(std::string_view name)
    {
        auto iter = std::find_if(attributes.begin(), attributes.end(), [name](Attribute& v) { return v.name == name; });
        return iter != attributes.end();
    }

    VertexAttributes& AddAttribute(const char* name, VertexAttributeSemantics semanticName, int semanticIndex, int size)
    {
        attributes.push_back(Attribute{name, semanticName, semanticIndex, size});
        return *this;
    }

    size_t GetSize() const { return data.size(); }

    auto GetData() -> const std::vector<uint8_t> { return data; }
    void SetData(const std::vector<uint8_t>& data) { this->data = data; }
    void SetData(std::vector<uint8_t>&& data) { this->data = std::move(data); }

    const std::vector<Attribute>& GetDescription() const { return attributes; }

    size_t GetAttributeHash() const;

    bool FindSemantics(VertexAttributeSemantics semantic, int index, Attribute& out) const
    {
        for (auto& a : attributes)
        {
            if (a.semanticName == semantic && a.semanticIndex == index)
            {
                out = a;
                return true;
            }
        }

        return false;
    }

private:
    std::vector<Attribute> attributes;
    mutable uint64_t hash = 0;

    // raw attribute data, attributes should be interleaved
    std::vector<uint8_t> data;

    void Rehash() const;
};
