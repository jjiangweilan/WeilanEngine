#pragma once
#include "Asset/Material.hpp"
#include "Core/GameObject.hpp"
#include "Graphics/Mesh.hpp"
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>

namespace Engine
{
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

private:
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Texture>> textures;
    std::vector<std::unique_ptr<Material>> materials;

    nlohmann::json jsonData;
    std::unordered_map<int, Mesh*> toOurMesh;
    std::unordered_map<int, Material*> toOurMaterial;
};
} // namespace Engine
