#include "TerrainSystem.hpp"
#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include <cstring>

std::unique_ptr<Mesh> TerrainSystem::CreateGridMesh(
    const float2& size,
    const float2& heightRange,
    uint32_t vertexResolution
)
{
    auto mesh = std::make_unique<Mesh>();
    if (size.x <= 0.0f || size.y <= 0.0f || heightRange.x >= heightRange.y || vertexResolution < 2)
        return mesh;

    const size_t vertexCount = static_cast<size_t>(vertexResolution) * vertexResolution;
    std::vector<glm::vec3> positions(vertexCount);
    std::vector<glm::vec2> uvs(vertexCount);

    for (uint32_t z = 0; z < vertexResolution; ++z)
    {
        const float v = static_cast<float>(z) / static_cast<float>(vertexResolution - 1);
        for (uint32_t x = 0; x < vertexResolution; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(vertexResolution - 1);
            const size_t index = static_cast<size_t>(z) * vertexResolution + x;
            positions[index] = glm::vec3((u - 0.5f) * size.x, 0.0f, (v - 0.5f) * size.y);
            uvs[index] = glm::vec2(u, v);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(vertexResolution - 1) * (vertexResolution - 1) * 6);
    for (uint32_t z = 0; z + 1 < vertexResolution; ++z)
    {
        for (uint32_t x = 0; x + 1 < vertexResolution; ++x)
        {
            const uint32_t v0 = z * vertexResolution + x;
            const uint32_t v1 = v0 + 1;
            const uint32_t v2 = (z + 1) * vertexResolution + x;
            const uint32_t v3 = v2 + 1;
            indices.insert(indices.end(), {v0, v2, v3, v0, v3, v1});
        }
    }

    VertexAttributes attributes;
    attributes.AddAttribute("texcoord", VertexAttributeSemantics::Texcoord, 0, sizeof(glm::vec2));
    std::vector<uint8_t> attributeData(uvs.size() * sizeof(glm::vec2));
    std::memcpy(attributeData.data(), uvs.data(), attributeData.size());
    attributes.SetData(std::move(attributeData));

    Submesh submesh;
    submesh.SetPositions(std::move(positions));
    submesh.SetIndices(std::move(indices));
    submesh.SetVertexAttribute(std::move(attributes));
    submesh.SetAABB(AABB(
        glm::vec3(-size.x * 0.5f, heightRange.x, -size.y * 0.5f),
        glm::vec3(size.x * 0.5f, heightRange.y, size.y * 0.5f)
    ));
    submesh.Apply();

    std::vector<Submesh> submeshes;
    submeshes.push_back(std::move(submesh));
    mesh->SetSubmeshes(std::move(submeshes));
    return mesh;
}
