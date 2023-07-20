#pragma once
#include "Core/Resource.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "Utils/Structs.hpp"
#include <string>
#include <vector>

namespace Engine
{
namespace Gfx
{
class Buffer;
} // namespace Gfx

struct DataRange
{
    uint32_t offsetInSrc;
    uint32_t size;
    void* data;
};

struct MeshBindingInfo
{
    uint32_t firstBinding = -1;
    RefPtr<Gfx::Buffer> indexBuffer;
    std::vector<Gfx::VertexBufferBinding> bindingBuffers;
    uint32_t indexBufferOffset = -1;
};

template <class T>
struct VertexAttribute
{
    std::vector<unsigned char> data;
    uint32_t count = 1;
    uint8_t componentCount = 1; // we only use float for vertex
};

struct UntypedVertexAttribute
{
    std::vector<unsigned char> data;
    uint8_t dataByteSize = 0;
    uint8_t componentCount = 1; // we only use float for vertex
    uint32_t count = 1;
};

struct VertexDescription
{
    VertexDescription() = default;
    VertexDescription(const VertexDescription& other) = default;
    VertexDescription(VertexDescription&& other)
        : position(std::move(other.position)), normal(std::move(other.normal)), tangent(std::move(other.tangent)),
          index(std::move(other.index)), texCoords(std::move(other.texCoords)), colors(std::move(other.colors)),
          joins(std::move(other.joins)), weights(std::move(other.weights))
    {}
    VertexAttribute<float> position;
    VertexAttribute<float> normal;
    VertexAttribute<float> tangent;
    UntypedVertexAttribute index;
    std::vector<UntypedVertexAttribute> texCoords;
    std::vector<UntypedVertexAttribute> colors;
    std::vector<UntypedVertexAttribute> joins;
    std::vector<UntypedVertexAttribute> weights;
};

class Mesh : public Resource
{
public:
    Mesh() {}
    Mesh(VertexDescription&& vertexDescription, const std::string& name = "", const UUID& uuid = UUID::empty);
    ~Mesh();

    const MeshBindingInfo& GetMeshBindingInfo() { return meshBindingInfo; }
    const VertexDescription& GetVertexDescription();
    const std::string& GetName() { return name; }
    Gfx::IndexBufferType GetIndexBufferType() { return indexBufferType; }

protected:
    MeshBindingInfo meshBindingInfo;
    Gfx::IndexBufferType indexBufferType;
    UniPtr<Gfx::Buffer> vertexBuffer;
    UniPtr<Gfx::Buffer> indexBuffer;
    VertexDescription vertexDescription;
    void UpdateMeshBindingInfo(std::vector<DataRange>& ranges);
    void GetAttributesDataRangesAndBufSize(std::vector<DataRange>& ranges, uint32_t& bufSize);

private:
    void OnMeshDataUploaded(void* data);
};
} // namespace Engine
