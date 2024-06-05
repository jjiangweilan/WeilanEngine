#include "JoltDebugRenderer.hpp"
#include "Rendering/Graphics.hpp"

void JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    Graphics::DrawLine(
        {inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()},
        {inTo.GetX(), inTo.GetY(), inTo.GetZ()},
        {inColor.r, inColor.g, inColor.b, inColor.a}
    );
}

JoltDebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    BatchImpl* batch = new BatchImpl();

    return batch;
}
JoltDebugRenderer::Batch JoltDebugRenderer::CreateTriangleBatch(
    const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount
)
{
    BatchImpl* batch = new BatchImpl();

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
{}

void JoltDebugRenderer::DrawText3D(
    JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight
)
{}
