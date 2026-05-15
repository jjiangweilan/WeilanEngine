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
    ObjPtr<Texture> grassShadowMask0;
    ObjPtr<Texture> grassShadowMask1;
    ObjPtr<Texture> windTex;
    float4 grassColorRamp_Bottom = {0.1f, 0.3f, 0.05f, 1.0f};
    float4 grassColorRamp_Top = {0.6f, 0.9f, 0.2f, 1.0f};
    float4 grassColorRamp2_Bottom = {0.1f, 0.3f, 0.05f, 1.0f};
    float4 grassColorRamp2_Top = {0.6f, 0.9f, 0.2f, 1.0f};
    float4 grassColorRamp3_Bottom = {0.1f, 0.3f, 0.05f, 1.0f};
    float4 grassColorRamp3_Top = {0.6f, 0.9f, 0.2f, 1.0f};
    float4 grassMaskUVScaler = {1.0f, 1.0f, 1.0f, 1.0f};
    float hueShift_0 = 0.0f;
    float hueShift_1 = 0.0f;
    float windScale = 1.0f;
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
