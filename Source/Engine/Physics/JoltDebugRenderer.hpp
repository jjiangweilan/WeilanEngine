#pragma once
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
// clang-format on
//

class Material;
class Mesh;
class JoltDebugRenderer : public JPH::DebugRenderer
{
public:
    static bool& GetDrawAll();

    JoltDebugRenderer()
    {
        Initialize();
    }
    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
    void DrawTriangle(
        JPH::RVec3Arg inV1,
        JPH::RVec3Arg inV2,
        JPH::RVec3Arg inV3,
        JPH::ColorArg inColor,
        ECastShadow inCastShadow = ECastShadow::Off
    ) override;

    Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;
    Batch CreateTriangleBatch(
        const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount
    ) override;

    void DrawGeometry(
        JPH::RMat44Arg inModelMatrix,
        const JPH::AABox& inWorldSpaceBounds,
        float inLODScaleSq,
        JPH::ColorArg inModelColor,
        const GeometryRef& inGeometry,
        ECullMode inCullMode = ECullMode::CullBackFace,
        ECastShadow inCastShadow = ECastShadow::On,
        EDrawMode inDrawMode = EDrawMode::Solid
    ) override;

    void DrawText3D(
        JPH::RVec3Arg inPosition,
        const JPH::string_view& inString,
        JPH::ColorArg inColor = JPH::Color::sWhite,
        float inHeight = 0.5f
    ) override;

    static std::unique_ptr<JoltDebugRenderer>& GetDebugRenderer();

private:
    class BatchImpl : public JPH::RefTargetVirtual
    {
    public:
        BatchImpl();
        ~BatchImpl();
        std::unique_ptr<Mesh> mesh;
        std::unique_ptr<Material> material;
        virtual void AddRef() override
        {
            refCount += 1;
        }
        virtual void Release() override
        {
            if (--refCount == 0)
                delete this;
        }

    private:
        size_t refCount = 0;
    };
};
