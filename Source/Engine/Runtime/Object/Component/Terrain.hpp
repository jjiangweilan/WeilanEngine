#pragma once

#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "MeshRenderer.hpp"
#include <memory>

class Terrain : public MeshRenderer
{
    DECLARE_OBJECT()

public:
    Terrain();
    Terrain(GameObject* owner);
    ~Terrain() override;

    void Serialize(Serializer* serializer) const override;
    void Deserialize(Serializer* serializer) override;
    const std::string& GetName() const override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    void Tick() override;
    void IdleTick() override;
    void OnLoaded() override;

    void SetTerrainConfig(TerrainConfig* config);
    TerrainConfig* GetTerrainConfig() const { return terrainConfig.Get(); }

    bool Raycast(
        const Ray& worldRay,
        float& outDistance,
        float3& outPoint,
        float3& outNormal
    ) const;

protected:
    void OnAwake() override;
    void OnDestroy() override;

private:
    ObjPtr<TerrainConfig> terrainConfig;
    std::unique_ptr<Material> terrainMaterial;
    uint64_t observedMeshRevision = 0;
    uint64_t observedMaterialRevision = 0;

    void RefreshResources(bool force);
    void RefreshMaterial();
};
