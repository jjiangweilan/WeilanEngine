#pragma once
#include "Core/GameObject.hpp"
#include "Graphics/Mesh.hpp"
#include "Rendering/Animation.hpp"
#include "Rendering/Material.hpp"
#include <glm/glm.hpp>
#include <span>
#include <string>

struct ModelNode
{
    struct MeshInfo
    {
        unsigned int index;
        unsigned int materialIndex;
    };
    std::string name;
    DynamicArray<MeshInfo> meshes;
    DynamicArray<ModelNode> children;
    glm::mat4 transform;
};

class Model : public Asset
{
    DECLARE_EXTERNAL_ASSET();

public:
    Model() {}
    Model(
        DynamicArray<GameObject*>&& rootGameObjects,
        DynamicArray<std::unique_ptr<GameObject>>&& gameObjects,
        DynamicArray<std::unique_ptr<Mesh>>&& meshes,
        DynamicArray<std::unique_ptr<Texture>>&& textures,
        DynamicArray<std::unique_ptr<Material>>&& materials,
        UUID uuid = UUID::GetEmptyUUID()
    )
        : meshes(std::move(meshes)), textures(std::move(textures)), materials(std::move(materials))
    {
        SetUUID(uuid);
    };

    RefPtr<Mesh> GetMesh(const std::string& name)
    {
        for (auto& m : meshes)
        {
            if (m->GetName() == name)
                return m;
        }

        return nullptr;
    }

    bool LoadFromFile(const char* path) override { return false; }

    DynamicArray<Asset*> GetInternalAssets() override;

    // the first one is the root object
    DynamicArray<std::unique_ptr<GameObject>> CreateGameObject();

    std::span<std::unique_ptr<Mesh>> GetMeshes() { return meshes; }
    std::span<std::unique_ptr<Texture>> GetTextures() { return textures; }
    std::span<std::unique_ptr<Material>> GetMaterials() { return materials; }
    std::span<std::unique_ptr<Animation>> GetAnimations() { return animations; }

    Material* GetDefaultMaterial();

    void SetModel(
        ModelNode root,
        DynamicArray<std::unique_ptr<Mesh>>&& meshes,
        DynamicArray<std::unique_ptr<Texture>>&& textures,
        DynamicArray<std::unique_ptr<Material>>&& materials,
        DynamicArray<std::unique_ptr<Animation>>&& animations
    );

private:
    bool assimpLoaded = false;
    DynamicArray<std::unique_ptr<Mesh>> meshes;
    DynamicArray<std::unique_ptr<Texture>> textures;
    DynamicArray<std::unique_ptr<Material>> materials;
    DynamicArray<std::unique_ptr<Animation>> animations;

    ModelNode rootNode;
    DynamicArray<std::unique_ptr<GameObject>> gameObjects; // the first one is the root

    nlohmann::json jsonData;
    std::unordered_map<int, Mesh*> toOurMesh;
    std::unordered_map<int, Material*> toOurMaterials;

    // per model default material
    std::unique_ptr<Material> material = nullptr;

    DynamicArray<std::unique_ptr<GameObject>> CreateGameObjectFromNode(
        nlohmann::json& j,
        int nodeIndex,
        std::unordered_map<int, Mesh*>& meshes,
        GameObject* parent,
        Material* defaultMaterial
    );

    DynamicArray<std::unique_ptr<GameObject>> CreateGameObject(ModelNode& n, GameObject* parent);
    void SetMaterialKeywords(ModelNode& node);

};
