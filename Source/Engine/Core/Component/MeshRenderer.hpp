#pragma once

#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/Animation.hpp"
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
    ~MeshRenderer() override {};

    // multi pass MeshRenderer draw the mesh multiple times using the materials
    // in pipeline it does:
    // 1. bind material 0
    //   draw mesh -- all submeshes
    // 2. bindg material 1
    //   draw mesh -- all submeshes
    // ...
    void EnableMultipass() { multipass = true; }

    void DisableMultipass() { multipass = false; }

    bool IsMultipassEnabled() { return multipass; }

    void SetMaterialSize(int size)
    {
        if (size >= 0)
        {
            materials.resize(size);
        }
    }

    int GetMaterialSize() { return materials.size(); }

    void Tick() override;
    void SetMeshes(std::span<Mesh*> meshes);
    void SetMesh(Mesh* mesh);
    void SetMaterials(std::span<Material*> materials);
    Mesh* GetMesh();
    std::span<Mesh*> GetMeshes();
    AABB GetAABB();
    void ValidateSkinning();
    void DisableSkinning();
    bool IsSkinningEnabled();
    const std::vector<Material*>& GetMaterials();
    Gfx::ShaderResource* GetObjectResource() { return gpuResource.get(); }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    /***** Serialized Data ******/
    std::vector<Mesh*> meshes;
    std::vector<Material*> materials = {};
    bool multipass = false;
    AABB aabb;
    bool wantsToEnableSkinning = false;

    /**** Runtime Data *******/
    std::unique_ptr<Gfx::ShaderResource> gpuResource;
    struct Skinning
    {
        static const int MaxBoneSize = 64;
        struct GPUBoneTransforms
        {
            glm::mat4 boneTrnasforms[MaxBoneSize];
        };
        bool enabled = false;
        std::vector<GameObject*> bones;
        std::vector<glm::mat4> offsetMatrix; // copy from mesh
        std::unique_ptr<Gfx::Buffer> bonesBuffer;
    } skinning;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();
    void UpdateAABB();
    void UpdateSkinning();

    void EnableImple() override;
    void DisableImple() override;
};
