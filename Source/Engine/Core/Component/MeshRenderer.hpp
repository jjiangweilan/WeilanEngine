#pragma once

#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Structs.hpp"
#include <memory>
class RenderingScene;
class MeshRenderer : public Component
{
    DECLARE_OBJECT();

public:
    MeshRenderer();
    MeshRenderer(GameObject* owner, Mesh* mesh, Material* material);
    MeshRenderer(GameObject* owner);
    ~MeshRenderer() override{};

    // multi pass MeshRenderer draw the mesh multiple times using the materials
    // in pipeline it does:
    // 1. bind material 0
    //   draw mesh -- all submeshes
    // 2. bindg material 1
    //   draw mesh -- all submeshes
    // ...
    void EnableMultipass()
    {
        multipass = true;
    }

    void DisableMultipass()
    {
        multipass = false;
    }

    bool IsMultipassEnabled()
    {
        return multipass;
    }

    void SetMaterialSize(int size)
    {
        if (size >= 0)
        {
            materials.resize(size);
        }
    }

    int GetMaterialSize()
    {
        return materials.size();
    }

    void SetMesh(Mesh* mesh);
    void SetMaterials(std::span<Material*> materials);
    Mesh* GetMesh();
    AABB GetAABB();
    const std::vector<Material*>& GetMaterials();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    Mesh* mesh = nullptr;
    std::vector<Material*> materials = {};
    bool multipass = false;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();

    void EnableImple() override;
    void DisableImple() override;
};
