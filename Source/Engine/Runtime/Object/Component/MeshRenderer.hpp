#pragma once

#include "Component.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Animation.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/Structs.hpp"
#include <functional>
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
    const std::vector<ObjPtr<Material>>& GetMaterials();
    Gfx::ShaderResource* GetObjectResource() { return gpuResource.get(); }

    void OnDrawGizmos() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() const override;
    void OnLoaded() override;

    // called by RenderingScene
    void UpdateSkinning();

    void EnableRayTracing(bool enabled);
    bool IsRayTracingEnabled() const { return isRayTracingEnabled; }

    // GPU-Driven rendering
    void SetGPUObject(bool enabled);
    bool IsGPUObject() const { return isGPUObject; }
    const Rendering::GpuObjectDescriptor& GetGpuObjectDescriptor() const
    {
        return gpuObjectDescriptor;
    }

    const Rendering::GpuRenderDataListDescriptor& GetGpuRenderDataListDescriptor() const
    {
        return gpuRenderDataListDescriptor;
    }

    const Rendering::GpuGeometryDescriptor& GetGpuGeometry(int index) const
    {
        if (index >= 0 && index < gpuGeometries.size())
            return gpuGeometries[index];

        static Rendering::GpuGeometryDescriptor g{};
        return g;
    }

private:
    /***** Serialized Data ******/
    std::vector<ObjPtr<Mesh>> meshes{};
    std::vector<ObjPtr<Material>> materials = {};
    bool multipass = false;
    AABB aabb{};
    AABB aabbWS{};
    bool wantsToEnableSkinning = false;
    bool isRayTracingEnabled = true;
    bool isGPUObject = true;

    /**** Runtime Data *******/
    bool isRayTracingInitialized = false;
    Gfx::RayTracingMeshHandle rayTracingMesh = -1;
    Gfx::RayTracingInstanceHandle rayTracingInstance = -1;

    // GPU-Driven handles (one per submesh)
    Rendering::GpuRenderDataListHandle renderDataListHandle;
    Rendering::GpuObjectHandle gpuObjectHandle;
    Rendering::GpuObjectDescriptor gpuObjectDescriptor;
    Rendering::GpuRenderDataListDescriptor gpuRenderDataListDescriptor;
    std::vector<Rendering::GpuGeometryDescriptor> gpuGeometries;
    bool gpuObjectRegistered = false;

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
        std::vector<GameObject*> bones = {};
        std::vector<glm::mat4> tposeMatrix = {}; // copy from mesh
        std::unique_ptr<Gfx::Buffer> bonesBuffer = nullptr;

        glm::vec3 rootMotionDelta;
    } skinning;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();
    void UpdateAABB();
    void CheckSkeleton();

    void OnStart() override;
    void OnEnable() override;
    void OnDisable() override;
    void TransformChanged() override;
    void InitializeForRayTracing();

    // GPU-Driven
    void RegisterGPUSceneObjects();
    void UnregisterGPUSceneObjects();
    void UpdateGPUSceneObjectTransforms();

    void ApplyToGPUSceneObjects(std::function<void(const Rendering::GpuObject&, int)> action);
};
