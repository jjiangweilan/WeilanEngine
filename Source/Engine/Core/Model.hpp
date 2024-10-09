#pragma once
#include "Core/GameObject.hpp"
#include "Graphics/Mesh.hpp"
#include "Rendering/Material.hpp"
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>

class Model : public Asset
{
    DECLARE_EXTERNAL_ASSET();

public:
    Model() {}
    Model(
        std::vector<GameObject*>&& rootGameObjects,
        std::vector<std::unique_ptr<GameObject>>&& gameObjects,
        std::vector<std::unique_ptr<Mesh>>&& meshes,
        std::vector<std::unique_ptr<Texture>>&& textures,
        std::vector<std::unique_ptr<Material>>&& materials,
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

    bool LoadFromFile(const char* path) override;

    std::vector<Asset*> GetInternalAssets() override;

    // the first one is the root object
    std::vector<std::unique_ptr<GameObject>> CreateGameObject();

    std::span<std::unique_ptr<Mesh>> GetMeshes()
    {
        return meshes;
    }
    std::span<std::unique_ptr<Texture>> GetTextures()
    {
        return textures;
    }
    std::span<std::unique_ptr<Material>> GetMaterials()
    {
        return materials;
    }

    Material* GetDefaultMaterial();

private:
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Texture>> textures;
    std::vector<std::unique_ptr<Material>> materials;

    nlohmann::json jsonData;
    std::unordered_map<int, Mesh*> toOurMesh;
    std::unordered_map<int, Material*> toOurMaterials;

    // per model default material
    std::unique_ptr<Material> material = nullptr;

    std::vector<std::unique_ptr<GameObject>> CreateGameObjectFromNode(
        nlohmann::json& j, int nodeIndex, std::unordered_map<int, Mesh*>& meshes, GameObject* parent, Material* defaultMaterial
    );
};
