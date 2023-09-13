#pragma once
#include "Core/Resource.hpp"
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
    Submesh(
        std::unique_ptr<unsigned char>&& vertexBuffer,
        std::vector<VertexBinding>&& bindings,
        std::unique_ptr<unsigned char>&& indexBuffer,
        Gfx::IndexBufferType indexBufferType,
        int indexCount,
        std::string_view name = ""
    );
    ~Submesh();

    int GetIndexCount()
    {
        return indexCount;
    }

    Gfx::IndexBufferType GetIndexBufferType()
    {
        return indexBufferType;
    }
    Gfx::Buffer* GetIndexBuffer()
    {
        return gfxIndexBuffer.get();
    }
    Gfx::Buffer* GetVertexBuffer()
    {
        return gfxVertexBuffer.get();
    }
    std::span<VertexBinding> GetBindings()
    {
        return bindings;
    }
    unsigned char* GetIndexBufferPtr()
    {
        return indexBuffer.get();
    }
    unsigned char* GetVertexBufferPtr()
    {
        return vertexBuffer.get();
    }

private:
    std::unique_ptr<Gfx::Buffer> gfxVertexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> gfxIndexBuffer = nullptr;
    std::unique_ptr<unsigned char> vertexBuffer = nullptr;
    std::unique_ptr<unsigned char> indexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType;
    std::vector<VertexBinding> bindings;
    int indexCount = 0;
    std::string name;
};

class Mesh2 : public Resource
{
    DECLARE_RESOURCE();

public:
    Mesh2(UUID uuid = UUID::empty) : Resource(), submeshes()
    {
        SetUUID(uuid);
    }
    ~Mesh2();

    std::vector<std::unique_ptr<Submesh>> submeshes;
};
} // namespace Engine
