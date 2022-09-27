#include "Mesh.hpp"
#include "GfxDriver/GfxFactory.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Code/Utils.hpp"
namespace Engine
{
    Mesh::Mesh(const VertexDescription& vertexDescription, const std::string& name, const UUID& uuid) :
        AssetObject(uuid),
        vertexDescription(vertexDescription)
    {
        this->name = name;
        uint32_t bufSize = 0;
        std::vector<DataRange> ranges;

        GetAttributesDataRangesAndBufSize(ranges, bufSize);
        vertexBuffer = Gfx::GfxFactory::Instance()->CreateBuffer(bufSize, Gfx::BufferUsage::Vertex);

        uint32_t indexBufSize = vertexDescription.index.count * sizeof(uint16_t);
        indexBuffer = Gfx::GfxFactory::Instance()->CreateBuffer(indexBufSize, Gfx::BufferUsage::Index);

        // load data to gpu buffer
        unsigned char* temp = new unsigned char[bufSize];

        for(auto& range : ranges)
        {
            memcpy(temp + range.offsetInSrc,
            range.data,
            range.size);
        }

        vertexBuffer->Write(temp, bufSize, 0);
        indexBuffer->Write(vertexDescription.index.data, indexBufSize, 0);

        delete[] temp;

        // update mesh binding
        UpdateMeshBindingInfo(ranges);
    }

    // Destructor has to be define here so that the auto generated destructor contains full defination of the type in our smart pointers
    Mesh::~Mesh() = default; 

    template<class T>
    uint32_t AddVertexAttribute(std::vector<DataRange>& ranges, VertexAttribute<T>& attr, uint32_t offset)
    {
        uint32_t size = 0;
        uint32_t alignmentPadding = Utils::GetPadding(offset, sizeof(T));

        if (attr.data != nullptr)
        {
            DataRange range;

            range.data = attr.data;
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

        for(auto& attr : attrs)
        {
            if (attr.data != nullptr)
            {
                uint32_t alignmentPadding = Utils::GetPadding(offset, attr.dataByteSize);
                DataRange range;
                range.data = attr.data;
                range.offsetInSrc = offset + size + alignmentPadding;
                range.size = attr.count * attr.dataByteSize * attr.componentCount;
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
        assert(vertexDescription.index.data != nullptr);

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
        meshBindingInfo.bindingOffsets.clear();

        meshBindingInfo.firstBinding = 0;

        for(uint32_t i = 0; i < ranges.size(); ++i)
        {
            auto& range = ranges[i];
            meshBindingInfo.bindingBuffers.push_back(vertexBuffer);
            meshBindingInfo.bindingOffsets.push_back(range.offsetInSrc);
        }

        meshBindingInfo.indexBuffer = indexBuffer;
        meshBindingInfo.indexBufferOffset = 0;
    }

    const VertexDescription& Mesh::GetVertexDescription()
    {
        return vertexDescription;
    }
}
