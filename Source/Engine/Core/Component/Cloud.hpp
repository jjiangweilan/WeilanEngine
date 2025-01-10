#pragma once

#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Structs.hpp"
#include <memory>
class RenderingScene;
class Cloud : public Component
{
    DECLARE_OBJECT();

public:
    Cloud();
    Cloud(GameObject* owner);
    ~Cloud() override {};

    void SetMesh(Mesh* mesh);
    Mesh* GetMesh();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void CreateCloudMaterials();
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

    ObjPtr<Material> GetVolumetricCloud() const { return volumetricCloud; }
    ObjPtr<Material> GetNoiseGenerator() const { return noiseGenerator; }

private:
    ObjPtr<Material> volumetricCloud;
    ObjPtr<Material> noiseGenerator;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();

    void OnEnable() override;
    void OnDisable() override;
};
