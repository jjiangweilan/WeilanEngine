#include "JoltDebugRenderer.hpp"
#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Graphics.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

namespace
{
glm::vec4 ToGlmColor(JPH::ColorArg color)
{
    return {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
}
} // namespace

void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    lineBatch.push_back({inFrom, inTo, inColor});
}

void JoltDebugRenderer::DrawTriangle(
    JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow
)
{
    Graphics::DrawTriangle(
        {inV1.GetX(), inV1.GetY(), inV1.GetZ()},
        {inV2.GetX(), inV2.GetY(), inV2.GetZ()},
        {inV3.GetX(), inV3.GetY(), inV3.GetZ()},
        ToGlmColor(inColor)
    );
}

JoltDebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    BatchImpl* batch = new BatchImpl();

    Submesh submesh;
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    VertexAttributes attributes;
    std::vector<uint8_t> attributeData;
    attributes.AddAttribute("normal", VertexAttributeSemantics::Normal, 0, 3 * sizeof(float));
    attributes.AddAttribute("color", VertexAttributeSemantics::Color, 0, 4 * sizeof(float));
    attributes.AddAttribute("uv", VertexAttributeSemantics::Texcoord, 0, 2 * sizeof(float));
    for (int i = 0; i < inTriangleCount; ++i)
    {
        auto& tri = inTriangles[i];

        for (int vi = 0; vi < 3; ++vi)
        {
            auto& vertexData = tri.mV[vi];
            positions.push_back({vertexData.mPosition.x, vertexData.mPosition.y, vertexData.mPosition.z});

            uint8_t data[9 * sizeof(float)];
            glm::vec3 normal = {vertexData.mNormal.x, vertexData.mNormal.y, vertexData.mNormal.z};
            glm::vec4 color = {vertexData.mColor.r, vertexData.mColor.g, vertexData.mColor.b, vertexData.mColor.a};
            glm::vec2 uv = {vertexData.mUV.x, vertexData.mUV.y};
            memcpy(data, &normal, 3 * sizeof(float));
            memcpy(data + 3 * sizeof(float), &color, 4 * sizeof(float));
            memcpy(data + (3 + 4) * sizeof(float), &uv, 2 * sizeof(float));

            attributeData.insert(attributeData.end(), data, data + 9 * sizeof(float));

            indices.push_back(i * 3 + vi);
        }
    }
    attributes.SetData(std::move(attributeData));

    submesh.SetIndices(std::move(indices));
    submesh.SetPositions(std::move(positions));
    submesh.SetVertexAttribute(std::move(attributes));
    submesh.Apply();
    std::vector<Submesh> submeshes;
    submeshes.push_back(std::move(submesh));
    mesh->SetSubmeshes(std::move(submeshes));
    batch->mesh = std::move(mesh);
    batch->material = std::make_unique<Material>(&EngineInternalResources::GetJoltDebugShader());

    return batch;
}
JoltDebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(
    const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount
)
{
    BatchImpl* batch = new BatchImpl();

    Submesh submesh;
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>();
    std::vector<glm::vec3> positions;
    std::vector<uint32_t> indices;
    VertexAttributes attributes;
    std::vector<uint8_t> attributeData;
    attributes.AddAttribute("normal", VertexAttributeSemantics::Normal, 0, 3 * sizeof(float));
    attributes.AddAttribute("color", VertexAttributeSemantics::Color, 0, 4 * sizeof(float));
    attributes.AddAttribute("uv", VertexAttributeSemantics::Texcoord, 0, 2 * sizeof(float));
    for (int vi = 0; vi < inVertexCount; ++vi)
    {
        auto& vertexData = inVertices[vi];
        positions.push_back({vertexData.mPosition.x, vertexData.mPosition.y, vertexData.mPosition.z});

        uint8_t data[9 * sizeof(float)];
        glm::vec3 normal = {vertexData.mNormal.x, vertexData.mNormal.y, vertexData.mNormal.z};
        glm::vec4 color = {vertexData.mColor.r, vertexData.mColor.g, vertexData.mColor.b, vertexData.mColor.a};
        glm::vec2 uv = {vertexData.mUV.x, vertexData.mUV.y};
        memcpy(data, &normal, 3 * sizeof(float));
        memcpy(data + 3 * sizeof(float), &color, 4 * sizeof(float));
        memcpy(data + (3 + 4) * sizeof(float), &uv, 2 * sizeof(float));

        attributeData.insert(attributeData.end(), data, data + 9 * sizeof(float));
    }
    attributes.SetData(std::move(attributeData));

    for (int ii = 0; ii < inIndexCount; ++ii)
    {
        indices.push_back(inIndices[ii]);
    }

    submesh.SetIndices(std::move(indices));
    submesh.SetPositions(std::move(positions));
    submesh.SetVertexAttribute(std::move(attributes));
    submesh.Apply();
    std::vector<Submesh> submeshes;
    submeshes.push_back(std::move(submesh));
    mesh->SetSubmeshes(std::move(submeshes));
    batch->mesh = std::move(mesh);
    batch->material = std::make_unique<Material>(&EngineInternalResources::GetJoltDebugShader());

    return batch;
}

void JoltDebugRenderer::DrawGeometry(
    JPH::RMat44Arg inModelMatrix,
    const JPH::AABox& inWorldSpaceBounds,
    float inLODScaleSq,
    JPH::ColorArg inModelColor,
    const GeometryRef& inGeometry,
    ECullMode inCullMode,
    ECastShadow inCastShadow,
    EDrawMode inDrawMode
)
{
    auto batchImpl = static_cast<BatchImpl*>(inGeometry->mLODs[0].mTriangleBatch.GetPtr());

    auto c0 = inModelMatrix.GetColumn4(0);
    auto c1 = inModelMatrix.GetColumn4(1);
    auto c2 = inModelMatrix.GetColumn4(2);
    auto c3 = inModelMatrix.GetColumn4(3);

    glm::mat4 model{
        {c0.GetX(), c0.GetY(), c0.GetZ(), c0.GetW()},
        {c1.GetX(), c1.GetY(), c1.GetZ(), c1.GetW()},
        {c2.GetX(), c2.GetY(), c2.GetZ(), c2.GetW()},
        {c3.GetX(), c3.GetY(), c3.GetZ(), c3.GetW()}
    };

    // all geometry is drawn in wire frame regardless of the settings
    // it's hard set in shader
    Graphics::DrawMesh(*batchImpl->mesh, 0, model, *batchImpl->material);
}

void JoltDebugRenderer::DrawText3D(
    JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight
)
{}

JoltDebugRenderer::BatchImpl::BatchImpl() {}
JoltDebugRenderer::BatchImpl::~BatchImpl() {}

std::unique_ptr<JoltDebugRenderer>& JoltDebugRenderer::GetDebugRenderer()
{
    static std::unique_ptr<JoltDebugRenderer> joltDebugRenderer;
    return joltDebugRenderer;
}

void JoltDebugRenderer::FlushLines()
{
    if (JoltDebugRenderer* renderer = GetDebugRenderer().get())
        renderer->FlushLineBatch();
}

void JoltDebugRenderer::FlushLineBatch()
{
    std::vector<Graphics::Line> lines;
    lines.reserve(lineBatch.size());
    for (const DebugLine& line : lineBatch)
    {
        lines.push_back(
            {
                {line.from.GetX(), line.from.GetY(), line.from.GetZ()},
                {line.to.GetX(), line.to.GetY(), line.to.GetZ()},
                ToGlmColor(line.color)
            }
        );
    }

    Graphics::DrawLines(lines);
    lineBatch.clear();
}

void JoltDebugRenderer::Init()
{
    GetDebugRenderer() = std::make_unique<JoltDebugRenderer>();
}

void JoltDebugRenderer::Deinit()
{
    GetDebugRenderer() = nullptr;
}
