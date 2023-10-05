#pragma once
#include "Core/Asset.hpp"
#include "GfxDriver/Buffer.hpp"
#include "Libs/Ptr.hpp"
#include "Utils/Structs.hpp"
#include <glm/glm.hpp>
#include <iterator>
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
    Submesh(Submesh&& other) = default;
    Submesh& operator=(Submesh&& other) = default;
    ~Submesh();

    inline int GetIndexCount() const
    {
        return indexCount;
    }

    Gfx::IndexBufferType GetIndexBufferType() const
    {
        return indexBufferType;
    }

    Gfx::Buffer* GetIndexBuffer() const
    {
        return gfxIndexBuffer.get();
    }

    Gfx::Buffer* GetVertexBuffer() const
    {
        return gfxVertexBuffer.get();
    }

    std::span<const VertexBinding> GetBindings() const
    {
        return bindings;
    }

    uint8_t* GetIndexBufferData() const
    {
        return indexBuffer.get();
    }

    uint8_t* GetVertexBufferData() const
    {
        return vertexBuffer.get();
    }

    const AABB& GetAABB() const;

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

class Mesh : public Asset
{
    DECLARE_ASSET();

public:
    Mesh() : Asset(), submeshes() {}
    Mesh(Mesh&& other) = default;
    ~Mesh();

    bool IsExternalAsset() override
    {
        return true;
    }

    bool LoadFromFile(const char* path) override;

    const std::vector<Submesh>& GetSubmeshes()
    {
        return submeshes;
    };

    void SetSubmeshes(std::vector<Submesh>&& submeshes)
    {
        this->submeshes = std::move(submeshes);
    }

private:
    std::vector<Submesh> submeshes;
    AABB aabb;
};
} // namespace Engine
