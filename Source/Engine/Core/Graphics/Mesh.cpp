#include "Mesh.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Utils.hpp"
#include "Rendering/GfxResourceTransfer.hpp"

using namespace Engine::Internal;
namespace Engine
{
DEFINE_ASSET(Mesh, "A96DED1A-AE44-451B-9531-73673DC12832", "MeshPlaceHolder");

Mesh::Mesh(VertexDescription&& vertexDescription, const std::string& name, const UUID& uuid)
    : vertexDescription(std::move(vertexDescription))
{
    SetUUID(uuid);
    this->name = name;
    uint32_t bufSize = 0;
    std::vector<DataRange> ranges;

    GetAttributesDataRangesAndBufSize(ranges, bufSize);
    Gfx::Buffer::CreateInfo bufCreateInfo;
    bufCreateInfo.size = bufSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Vertex | Gfx::BufferUsage::Transfer_Dst;
    bufCreateInfo.debugName = name.c_str();
    vertexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    uint32_t indexBufSize = this->vertexDescription.index.count * vertexDescription.index.dataByteSize;
    bufCreateInfo.size = indexBufSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Index | Gfx::BufferUsage::Transfer_Dst;
    bufCreateInfo.debugName = name.c_str();
    indexBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);

    // load data to gpu buffer
    unsigned char* temp = new unsigned char[bufSize];

    for (auto& range : ranges)
    {
        memcpy(temp + range.offsetInSrc, range.data, range.size);
    }

    GfxResourceTransfer::BufferTransferRequest request0{.data = temp, .bufOffset = 0, .size = bufSize};
    GetGfxResourceTransfer()->Transfer(vertexBuffer, request0);
    delete[] temp;

    GfxResourceTransfer::BufferTransferRequest request1{
        .data = this->vertexDescription.index.data.data(),
        .bufOffset = 0,
        .size = indexBufSize,
    };
    GetGfxResourceTransfer()->Transfer(indexBuffer, request1);

    if (vertexDescription.index.dataByteSize == 2)
        indexBufferType = Gfx::IndexBufferType::UInt16;
    else if (vertexDescription.index.dataByteSize == 4)
        indexBufferType = Gfx::IndexBufferType::UInt32;
    else
        assert(0);

    // update mesh binding
    UpdateMeshBindingInfo(ranges);
}

// Destructor has to be define here so that the auto generated destructor contains full defination of the type in our
// smart pointers
Mesh::~Mesh() = default;

template <class T>
uint32_t AddVertexAttribute(std::vector<DataRange>& ranges, VertexAttribute<T>& attr, uint32_t offset)
{
    uint32_t size = 0;
    uint32_t alignmentPadding = Utils::GetPadding(offset, sizeof(T));

    if (!attr.data.empty())
    {
        DataRange range;

        range.data = attr.data.data();
        range.offsetInSrc = offset + alignmentPadding;
        range.size = attr.count * sizeof(T) * attr.componentCount;
        size += (range.size + alignmentPadding);
        ranges.push_back(range);
    }

    return size;
}

uint32_t AddVertexAttribute(std::vector<DataRange>& ranges, std::vector<UntypedVertexAttribute>& attrs, uint32_t offset)
{
    uint32_t size = 0;

    for (auto& attr : attrs)
    {
        if (!attr.data.empty())
        {
            uint32_t alignmentPadding = Utils::GetPadding(offset, attr.dataByteSize);
            DataRange range;
            range.offsetInSrc = offset + size + alignmentPadding;
            range.size = attr.count * attr.dataByteSize * attr.componentCount;
            range.data = attr.data.data();
            size += (range.size + alignmentPadding);
            ranges.push_back(range);
        }
    }
    return size;
}

void Mesh::GetAttributesDataRangesAndBufSize(std::vector<DataRange>& ranges, uint32_t& bufSize)
{
    ranges.clear();
    bufSize = 0;
    assert(!vertexDescription.index.data.empty());

    bufSize += AddVertexAttribute(ranges, vertexDescription.position, bufSize);
    bufSize += AddVertexAttribute(ranges, vertexDescription.normal, bufSize);
    bufSize += AddVertexAttribute(ranges, vertexDescription.tangent, bufSize);
    bufSize += AddVertexAttribute(ranges, vertexDescription.texCoords, bufSize);
    bufSize += AddVertexAttribute(ranges, vertexDescription.colors, bufSize);
    bufSize += AddVertexAttribute(ranges, vertexDescription.joins, bufSize);
    bufSize += AddVertexAttribute(ranges, vertexDescription.weights, bufSize);
}

void Mesh::UpdateMeshBindingInfo(std::vector<DataRange>& ranges)
{
    meshBindingInfo.bindingBuffers.clear();

    meshBindingInfo.firstBinding = 0;

    for (uint32_t i = 0; i < ranges.size(); ++i)
    {
        auto& range = ranges[i];
        meshBindingInfo.bindingBuffers.push_back({vertexBuffer.Get(), range.offsetInSrc});
    }

    meshBindingInfo.indexBuffer = indexBuffer;
    meshBindingInfo.indexBufferOffset = 0;
}

const VertexDescription& Mesh::GetVertexDescription()
{
    return vertexDescription;
}

} // namespace Engine
