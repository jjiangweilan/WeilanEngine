#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"
#include "Engine/Driver/GfxDriver/VertexBufferBinding.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include <algorithm>
#include <glm/glm.hpp>

#include "Engine/Library/DynamicArray.hpp"
#include <string_view>

// interleaving or not? mix them? https://developer.arm.com/documentation/102546/0100/Index-Driven-Geometry-Pipeline
//

struct SkeletonBone
{
    std::string name;
    glm::mat4 offsetMatrix;
};

using Skeleton = std::vector<SkeletonBone>;

class Submesh
{
    // general version API
public:
    Submesh() {}
    Submesh(Submesh&& other) = default;
    Submesh& operator=(Submesh&& other) = default;
    ~Submesh();

    inline int GetIndexCount() const { return indexCount; }
    int GetTriangleCount() const { return indexCount / 3; }

    uint32_t GetVertexDataByteSize() const;
    uint32_t GetIndexDataByteSize() const;

    Gfx::IndexBufferType GetIndexBufferType() const { return indexBufferType; }

    const AABB& GetAABB() const;
    void SetAABB(const AABB& aabb);

    Gfx::Buffer* GetIndexBuffer() const { return gfxIndexBuffer.get(); }

    Gfx::Buffer* GetVertexBuffer() const { return gfxVertexBuffer.get(); }

    std::span<const VertexBinding> GetBindings() const { return bindings; }

    std::span<const Gfx::VertexBufferBinding> GetGfxVertexBufferBindings() const { return gfxBindings; }

public:
    void SetIndices(std::vector<uint32_t>&& indices);
    void SetIndices(const std::vector<uint32_t>& indices);
    void SetPositions(std::vector<glm::vec3>&& positions);
    void SetVertexAttribute(VertexAttributes&& vertAttributes);
    void SetVertexAttribute(const VertexAttributes& vertAttributes);
    void SetPositions(const std::vector<glm::vec3>& positions);
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

    const std::vector<uint32_t>& GetIndices() const;
    const std::vector<glm::vec3>& GetPositions() const;
    const VertexAttributes& GetAttribute() const;

private:
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> positions; // binding 0,
    VertexAttributes attributes;      // binding 1, interleaved
                                      //
    std::unique_ptr<Gfx::Buffer> gfxVertexBuffer = nullptr;
    std::unique_ptr<Gfx::Buffer> gfxIndexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType = Gfx::IndexBufferType::UInt32;
    std::vector<VertexBinding> bindings;
    std::vector<Gfx::VertexBufferBinding> gfxBindings;
    AABB aabb;
    int indexCount = 0;
    std::string name;
    uint32_t vertexBufferSize;
    uint32_t indexBufferSize;

    void UpdateVertexDataByteSize();
    void UpdateIndexDataByteSize();
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

    const std::vector<Submesh>& GetSubmeshes() { return submeshes; };

    Submesh* GetSubmesh(int index)
    {
        if (index < submeshes.size())
        {
            return &submeshes[index];
        }
        return nullptr;
    };

    void SetSubmeshes(std::vector<Submesh>&& submeshes)
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
    std::vector<Submesh> submeshes = {};
    Skeleton skeleton = {};
    AABB aabb = {{0, 0, 0}, {0, 0, 0}};
};
