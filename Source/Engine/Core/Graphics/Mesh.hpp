#pragma once
#include "Core/Asset.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/VertexAttributes.hpp"
#include "GfxDriver/VertexBufferBinding.hpp"
#include "Rendering/Structs.hpp"
#include <algorithm>
#include <glm/glm.hpp>

#include <string_view>
#include "Libs/DynamicArray.hpp"

// interleaving or not? mix them? https://developer.arm.com/documentation/102546/0100/Index-Driven-Geometry-Pipeline
//

struct SkeletonBone
{
    std::string name;
    glm::mat4 offsetMatrix;
};

using Skeleton = DynamicArray<SkeletonBone>;

class Submesh
{
    // general version API
public:
    Submesh() {}
    Submesh(Submesh&& other) = default;
    Submesh& operator=(Submesh&& other) = default;
    ~Submesh();

    inline int GetIndexCount() const { return indexCount; }

    Gfx::IndexBufferType GetIndexBufferType() const { return indexBufferType; }

    const AABB& GetAABB() const;
    void SetAABB(const AABB& aabb);

    Gfx::Buffer* GetIndexBuffer() const { return gfxIndexBuffer.get(); }

    Gfx::Buffer* GetVertexBuffer() const { return gfxVertexBuffer.get(); }

    std::span<const VertexBinding> GetBindings() const { return bindings; }

    std::span<const Gfx::VertexBufferBinding> GetGfxVertexBufferBindings() const { return gfxBindings; }

private:
    std::unique_ptr<Gfx::Buffer> gfxVertexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> gfxIndexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType = Gfx::IndexBufferType::UInt32;
    DynamicArray<VertexBinding> bindings;
    DynamicArray<Gfx::VertexBufferBinding> gfxBindings;
    AABB aabb;
    int indexCount = 0;
    std::string name;

    // v0.2 API
public:
    void SetIndices(DynamicArray<uint32_t>&& indices);
    void SetIndices(const DynamicArray<uint32_t>& indices);
    void SetPositions(DynamicArray<glm::vec3>&& positions);
    void SetVertexAttribute(VertexAttributes&& vertAttributes);
    void SetVertexAttribute(const VertexAttributes& vertAttributes);
    void SetPositions(const DynamicArray<glm::vec3>& positions);
    void Apply();
    const VertexAttributes& GetVertexAttribute() const { return attributes; }
    bool HasAttribute(std::string_view name) const
    {
        for (auto& attr : attributes.GetDescription())
        {
            if (attr.name == name)
            {
                return true;
            }
        }
        return false;
    }

    const DynamicArray<uint32_t>& GetIndices() const;
    const DynamicArray<glm::vec3>& GetPositions() const;
    const VertexAttributes& GetAttribute() const;

private:
    DynamicArray<uint32_t> indices;
    DynamicArray<glm::vec3> positions; // binding 0,
    VertexAttributes attributes;      // binding 1, interleaved

    // v0.1 API
public:
    Submesh(
        std::unique_ptr<unsigned char>&& vertexBuffer,
        DynamicArray<VertexBinding>&& bindings,
        std::unique_ptr<unsigned char>&& indexBuffer,
        Gfx::IndexBufferType indexBufferType,
        int indexCount,
        std::string_view name = ""
    );

    uint8_t* GetIndexBufferData() const { return indexBuffer.get(); }

    uint8_t* GetVertexBufferData() const { return vertexBuffer.get(); }

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

    bool IsExternalAsset() override { return true; }

    const AABB& GetAABB() const;

    bool LoadFromFile(const char* path) override;

    const DynamicArray<Submesh>& GetSubmeshes() { return submeshes; };

    Submesh* GetSubmesh(int index)
    {
        if (index < submeshes.size())
        {
            return &submeshes[index];
        }
        return nullptr;
    };

    void SetSubmeshes(DynamicArray<Submesh>&& submeshes)
    {
        this->submeshes = std::move(submeshes);

        glm::vec3 min =
            {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        glm::vec3 max = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        for (auto& submesh : this->submeshes)
        {
            auto& aabb = submesh.GetAABB();
            min = glm::min(min, aabb.min);
            max = glm::max(max, aabb.max);
        }

        aabb = {min, max};
    }

    void SetSkeleton(Skeleton skeleton) { this->skeleton = skeleton; }
    const Skeleton& GetSkeleton() { return skeleton; }
    bool HasSkeleton() const { return !skeleton.empty(); }

private:
    DynamicArray<Submesh> submeshes = {};
    Skeleton skeleton = {};
    AABB aabb = {{0, 0, 0}, {0, 0, 0}};
};
