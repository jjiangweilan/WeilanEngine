#pragma once
#include "Core/AssetObject.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Libs/Ptr.hpp"
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

namespace Engine
{
struct VertexBinding
{
    std::size_t byteOffset;
    std::size_t byteSize;
    std::string name;
};

class Submesh
{
public:
    Submesh() {}
    Submesh(UniPtr<unsigned char>&& vertexBuffer,
            std::vector<VertexBinding>&& bindings,
            UniPtr<unsigned char>&& indexBuffer,
            Gfx::IndexBufferType indexBufferType,
            int indexCount,
            std::string_view name = "");

    int GetIndexCount() { return indexCount; }

    Gfx::IndexBufferType GetIndexBufferType() { return indexBufferType; }
    Gfx::Buffer* GetIndexBuffer() { return gfxIndexBuffer.Get(); }
    Gfx::Buffer* GetVertexBuffer() { return gfxVertexBuffer.Get(); }
    std::span<VertexBinding> GetBindings() { return bindings; }
    unsigned char* GetIndexBufferPtr() { return indexBuffer.Get(); }
    unsigned char* GetVertexBufferPtr() { return vertexBuffer.Get(); }

private:
    UniPtr<Gfx::Buffer> gfxVertexBuffer = nullptr;
    UniPtr<Gfx::Buffer> gfxIndexBuffer = nullptr;
    UniPtr<unsigned char> vertexBuffer = nullptr;
    UniPtr<unsigned char> indexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType;
    std::vector<VertexBinding> bindings;
    int indexCount = 0;
    std::string name;
};

class Mesh2 : public AssetObject
{
public:
    Mesh2(UUID uuid = UUID::empty) : AssetObject(uuid), submeshes() {}
    std::vector<UniPtr<Submesh>> submeshes;

private:
    bool Serialize(AssetSerializer&) override { return false; } // disable model saving
};
} // namespace Engine
