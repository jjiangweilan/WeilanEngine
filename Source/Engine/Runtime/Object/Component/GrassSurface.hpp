#pragma once
#include "RenderingComponent.hpp"

struct [[SerClass]] GrassPatch
{
    // patch instance data
    float3 position;

    // mesh
    int meshIndex;
};

struct [[SerClass]] GrassConfig
{
    float3 albedo = {0.25f, 0.65f, 0.18f};
    float scale = 1.0f;
};

struct [[SerClass]] GrassPatchGroup
{
public:
    [[SerProp]] std::vector<ObjPtr<Mesh>> patchMeshes;
    [[SerProp]] std::vector<GrassPatch> patches;
    [[SerProp]] GrassConfig config;
};

class GrassSurface : public RenderingComponent<GrassSurface>
{
    DECLARE_RENDERING_COMPONENT(GrassSurface);

public:
    void OnEnable() override;
    void OnDisable() override;
    void Serialize(Serializer* ser) const override;
    void Deserialize(Serializer* ser) override;
    void OnDrawGizmos(GizmoManager& manager) override;

    GrassPatchGroup grassPatchGroup;

    void Tick() override;
};
