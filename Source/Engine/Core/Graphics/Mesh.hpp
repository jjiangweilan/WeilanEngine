#pragma once
#include "Core/AssetObject.hpp"
#include "Utils/Structs.hpp"
#include <vector>
#include <string>

namespace Engine
{
namespace Gfx
{
    class GfxBuffer;
} // namespace Engine::Gfx

    struct DataRange
    {
        uint32_t offsetInSrc;
        uint32_t size;
        void* data;
    };

    struct MeshBindingInfo
    {
        uint32_t firstBinding;
        RefPtr<Gfx::GfxBuffer> indexBuffer;
        uint32_t indexBufferOffset;
        std::vector<RefPtr<Gfx::GfxBuffer>> bindingBuffers;
        std::vector<uint64_t> bindingOffsets;
    };

    template<class T>
    struct VertexAttribute
    {
        T* data = nullptr;
        uint32_t count = 1;
        uint8_t componentCount = 1; // we only use float for vertex
    };

    struct UntypedVertexAttribute
    {
        void* data = nullptr;
        uint8_t dataByteSize = 0;
        uint8_t componentCount = 1; // we only use float for vertex
        uint32_t count = 1;
    };

    struct VertexDescription
    {
        VertexAttribute<float> position;
        VertexAttribute<float> normal;
        VertexAttribute<float> tangent;
        VertexAttribute<uint16_t> index;
        std::vector<UntypedVertexAttribute> texCoords;
        std::vector<UntypedVertexAttribute> colors;
        std::vector<UntypedVertexAttribute> joins;
        std::vector<UntypedVertexAttribute> weights;
    };

    class Mesh : public AssetObject
    {
        public:
            Mesh() {}
            Mesh(const VertexDescription& vertexDescription, const std::string& name = "", const UUID& uuid = UUID::empty);
            ~Mesh();
            void UpdateVertexData(float* data);

            const MeshBindingInfo& GetMeshBindingInfo() {return meshBindingInfo;}
            const VertexDescription& GetVertexDescription();
            const std::string& GetName() {return name;}
        protected:

            MeshBindingInfo meshBindingInfo;
            UniPtr<Gfx::GfxBuffer> vertexBuffer;
            UniPtr<Gfx::GfxBuffer> indexBuffer;
            VertexDescription vertexDescription;
            void UpdateMeshBindingInfo(std::vector<DataRange>& ranges);
            void GetAttributesDataRangesAndBufSize(std::vector<DataRange>& ranges, uint32_t& bufSize);
    };
}
