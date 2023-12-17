#pragma once

#include "Asset/Material.hpp"
#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
namespace Engine
{
class RenderingScene;
class MeshRenderer : public Component
{
    DECLARE_OBJECT();

public:
    MeshRenderer();
    MeshRenderer(GameObject* owner, Mesh* mesh, Material* material);
    MeshRenderer(GameObject* owner);
    ~MeshRenderer() override
    {
        RemoveFromRenderingScene();
    };

    void SetMesh(Mesh* mesh);
    void SetMaterials(std::span<Material*> materials);
    Mesh* GetMesh();
    const std::vector<Material*>& GetMaterials();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    Mesh* mesh = nullptr;
    RenderingScene* renderingScene = nullptr;
    std::vector<Material*> materials = {};
    AABB aabb;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();
    void NotifyGameObjectGameSceneSet() override;
};
} // namespace Engine
