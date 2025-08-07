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
    void SetMaterials(std::span<ObjPtr<Material>> materials);
    void SetMaterials(std::span<Material*> materials);
    void SetMaterial(Material* material);
    Mesh* GetMesh();
    std::span<ObjPtr<Mesh>> GetMeshes();
    AABB GetAABB();
    void ValidateSkinning();
    void DisableSkinning();
    bool IsSkinningEnabled();
    const DynamicArray<ObjPtr<Material>>& GetMaterials();
    Gfx::ShaderResource* GetObjectResource() { return gpuResource.get(); }

    void OnDrawGizmos() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

    // called by RenderingScene
    void UpdateSkinning();

private:
    /***** Serialized Data ******/
    DynamicArray<ObjPtr<Mesh>> meshes{};
    DynamicArray<ObjPtr<Material>> materials = {};
    bool multipass = false;
    AABB aabb {};
    AABB aabbWS {};
    bool wantsToEnableSkinning = false;

    /**** Runtime Data *******/
    bool hasSkeleton = false;
    bool aabbBoundsNeedUpdate = true;
    bool aabbPositionNeedUpdate = true;
    std::unique_ptr<Gfx::ShaderResource> gpuResource;
    struct Skinning
    {
        static const int MaxBoneSize = 256;
        struct GPUBoneTransforms
        {
            std::array<glm::mat4, MaxBoneSize> boneTrnasforms;
        };
        bool enabled = false;
        DynamicArray<GameObject*> bones = {};
        DynamicArray<glm::mat4> tposeMatrix = {}; // copy from mesh
        std::unique_ptr<Gfx::Buffer> bonesBuffer = nullptr;

        glm::vec3 rootMotionDelta;
    } skinning;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();
    void UpdateAABB();

    void OnEnable() override;
    void OnDisable() override;
    void TransformChanged() override;
};
