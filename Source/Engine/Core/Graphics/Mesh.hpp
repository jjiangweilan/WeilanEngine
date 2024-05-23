#pragma once
#include "Core/Asset.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/VertexBufferBinding.hpp"
#include "Libs/Ptr.hpp"
#include "Utils/Structs.hpp"
#include <glm/glm.hpp>
#include <iterator>
#include <string_view>
#include <vector>

// interleaving or not? mix them? https://developer.arm.com/documentation/102546/0100/Index-Driven-Geometry-Pipeline

struct VertexBinding
{
    std::size_t byteOffset;
    std::size_t byteSize;
    std::string name;
};

class VertexAttribute
{
public:
    struct Attribute
    {
        std::string name;
        int size;
    };

    VertexAttribute& AddAttribute(const char* name, int size)
    {
        attributes.push_back(Attribute{name, size});
        return *this;
    }

    size_t GetSize() const
    {
        return data.size();
    }

    const std::vector<uint8_t> GetData()
    {
        return data;
    }

    void SetData(const std::vector<uint8_t>& data)
    {
        this->data = data;
    }

    void SetData(std::vector<uint8_t>&& data)
    {
        this->data = std::move(data);
    }

    const std::vector<Attribute>& GetDescription() const
    {
        return attributes;
    }

private:
    std::vector<Attribute> attributes;

    // raw attribute data, attributes should be interleaved
    std::vector<uint8_t> data;
};

class Submesh
{
    // general version API
public:
    Submesh() {}
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

    const AABB& GetAABB() const;
    void SetAABB(const AABB& aabb);

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

    std::span<const Gfx::VertexBufferBinding> GetGfxVertexBufferBindings() const
    {
        return gfxBindings;
    }

private:
    std::unique_ptr<Gfx::Buffer> gfxVertexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> gfxIndexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType = Gfx::IndexBufferType::UInt16;
    std::vector<VertexBinding> bindings;
    std::vector<Gfx::VertexBufferBinding> gfxBindings;
    AABB aabb;
    int indexCount = 0;
    std::string name;

    // v0.2 API
public:
    void SetIndices(std::vector<uint32_t>&& indices);
    void SetIndices(const std::vector<uint32_t>& indices);
    void SetPositions(std::vector<glm::vec3>&& positions);
    void SetVertexAttribute(VertexAttribute&& vertAttributes);
    void SetVertexAttribute(const VertexAttribute& vertAttributes);
    void SetPositions(const std::vector<glm::vec3>& positions);
    void Apply();

    const std::vector<uint32_t>& GetIndices() const;
    const std::vector<glm::vec3>& GetPositions() const;
    const VertexAttribute& GetAttribute() const;

private:
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> positions; // binding 0,
    VertexAttribute attributes;       // binding 1, interleaved

    // v0.1 API
public:
    Submesh(
        std::unique_ptr<unsigned char>&& vertexBuffer,
        std::vector<VertexBinding>&& bindings,
        std::unique_ptr<unsigned char>&& indexBuffer,
        Gfx::IndexBufferType indexBufferType,
        int indexCount,
        std::string_view name = ""
    );

    uint8_t* GetIndexBufferData() const
    {
        return indexBuffer.get();
    }

    uint8_t* GetVertexBufferData() const
    {
        return vertexBuffer.get();
    }

private:
    std::unique_ptr<unsigned char> vertexBuffer = nullptr;
    std::unique_ptr<unsigned char> indexBuffer = nullptr;
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

    const AABB& GetAABB() const;

    bool LoadFromFile(const char* path) override;

    const std::vector<Submesh>& GetSubmeshes()
    {
        return submeshes;
    };

    void SetSubmeshes(std::vector<Submesh>&& submeshes)
    {
        this->submeshes = std::move(submeshes);

        glm::vec3 min = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
        glm::vec3 max = {
            std::numeric_limits<float>::min(),
            std::numeric_limits<float>::min(),
            std::numeric_limits<float>::min()};

        for (auto& submesh : submeshes)
        {
            auto& aabb = submesh.GetAABB();
            min.x = glm::min(min.x, aabb.min.x);
            min.y = glm::min(min.y, aabb.min.y);
            min.z = glm::min(min.z, aabb.min.z);
            max.x = glm::max(max.x, aabb.max.x);
            max.y = glm::max(max.y, aabb.max.y);
            max.z = glm::max(max.z, aabb.max.z);
        }

        aabb = {min, max};
    }

private:
    std::vector<Submesh> submeshes;
    AABB aabb;
};
