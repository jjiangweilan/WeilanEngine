#pragma once
#include "RenderingComponent.hpp"

struct [[SerClass]] GrassPatch
{
    [[SerProp]] float3 position;
    [[SerProp]] int meshIndex;
};

struct [[SerClass]] GrassPatchGroup
{
public:
    [[SerProp]] std::vector<ObjPtr<Mesh>> patchMeshes;
    [[SerProp]] std::vector<GrassPatch> patches;
};

class GrassSurface : public RenderingComponent<GrassSurface>
{
    DECLARE_RENDERING_COMPONENT(GrassSurface);

public:
    Material computeDispatchMat;
    Material drawMat;

    std::unique_ptr<Gfx::Buffer> grassDispatcherIndirectDrawBuffer;

    void Serialize(Serializer* ser) const override;
    void Deserialize(Serializer* ser) override;
    void OnDrawGizmos(GizmoManager& manager) override;

    GrassPatchGroup grassPatchGroup;

    void Tick() override;
};
