#include "GeometryUtils.hpp"

namespace Rendering
{
std::unique_ptr<Mesh> GeneratePlane(int width, int height, int vertexCountX, int vertexCountY)
{
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    if (width <= 0 || height <= 0 || vertexCountX < 2 || vertexCountY < 2)
    {
        return mesh; // empty mesh
    }

    // vertexCountX / vertexCountY are the number of vertices along X/Y.
    const int vertCountX = vertexCountX;
    const int vertCountY = vertexCountY;
    const int vertexCount = vertCountX * vertCountY;

    std::vector<glm::vec3> positions(vertexCount);
    std::vector<glm::vec2> uvs(vertexCount);

    // Generate grid vertices on XY plane (Z = 0), normal (0,0,1)
    // Plane spans [0,width] x [0,height]
    for (int y = 0; y < vertCountY; ++y)
    {
        for (int x = 0; x < vertCountX; ++x)
        {
            int idx = y * vertCountX + x;
            float fx = (float)x / (float)(vertCountX - 1);
            float fy = (float)y / (float)(vertCountY - 1);
            // Centered plane: shift by half width/height so it spans [-width/2, width/2] x [-height/2, height/2]
            positions[idx] = glm::vec3(fx * width - width * 0.5f, fy * height - height * 0.5f, 0.0f);
            uvs[idx] = glm::vec2(fx, fy);
        }
    }

    // Indices (two triangles per quad, CCW winding to give +Z normal)
    std::vector<uint32_t> indices;
    indices.reserve((vertCountX - 1) * (vertCountY - 1) * 6);
    for (int y = 0; y < vertCountY - 1; ++y)
    {
        for (int x = 0; x < vertCountX - 1; ++x)
        {
            uint32_t v0 = y * vertCountX + x;
            uint32_t v1 = v0 + 1;
            uint32_t v2 = (y + 1) * vertCountX + x;
            uint32_t v3 = v2 + 1;
            // Triangle 1: v0, v1, v2
            indices.push_back(v0);
            indices.push_back(v1);
            indices.push_back(v2);
            // Triangle 2: v1, v3, v2
            indices.push_back(v1);
            indices.push_back(v3);
            indices.push_back(v2);
        }
    }

    // Build vertex attributes (only texcoord as requested)
    VertexAttributes attributes;
    attributes.AddAttribute("texcoord", VertexAttributeSemantics::Texcoord, 0, sizeof(glm::vec2));
    std::vector<uint8_t> attrData(uvs.size() * sizeof(glm::vec2));
    memcpy(attrData.data(), uvs.data(), attrData.size());
    attributes.SetData(std::move(attrData));

    Submesh submesh;
    submesh.SetPositions(std::move(positions));
    submesh.SetIndices(std::move(indices));
    submesh.SetVertexAttribute(std::move(attributes));

    // AABB in local space.
    AABB aabb(glm::vec3(-0.5f * width, -0.5f * height, 0.0f), glm::vec3(0.5f * width, 0.5f * height, 0.0f));
    submesh.SetAABB(aabb);

    submesh.Apply();

    std::vector<Submesh> submeshes;
    submeshes.push_back(std::move(submesh));
    mesh->SetSubmeshes(std::move(submeshes));
    return mesh;
}

} // namespace Rendering
