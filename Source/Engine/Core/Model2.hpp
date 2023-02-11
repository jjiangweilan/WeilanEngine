#include "Core/GameObject.hpp"
#include "Core/Graphics/Material.hpp"
#include "Graphics/Mesh2.hpp"
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>

namespace Engine
{
class Model2 : public AssetObject
{
public:
    Model2(std::vector<GameObject*>&& rootGameObjects,
           std::vector<UniPtr<GameObject>>&& gameObjects,
           std::vector<UniPtr<Mesh2>>&& meshes,
           std::vector<UniPtr<Texture>> textures,
           std::vector<UniPtr<Material>> materials,
           UUID uuid = UUID::empty)
        : AssetObject(uuid), rootGameObjects(std::move(rootGameObjects)), gameObjects(std::move(gameObjects)),
          meshes(std::move(meshes)), textures(std::move(textures)), materials(std::move(materials))
    {
        SerializeMember("meshes", this->meshes);
        SerializeMember("textures", this->textures);
        SerializeMember("materials", this->materials);
        SerializeMember("gameObjects", this->gameObjects);
        SerializeMember("rootGameObjects", this->rootGameObjects);
    };

    RefPtr<Mesh2> GetMesh(const std::string& name)
    {
        for (auto& m : meshes)
        {
            if (m->GetName() == name)
                return m;
        }

        return nullptr;
    }

    std::span<GameObject*> GetRootGameObject() { return rootGameObjects; }
    std::span<UniPtr<GameObject>> GetGameObject() { return gameObjects; }
    std::span<UniPtr<Mesh2>> GetMeshes() { return meshes; }
    std::span<UniPtr<Texture>> GetTextures() { return textures; }
    std::span<UniPtr<Material>> GetMaterials() { return materials; }

private:
    bool Serialize(AssetSerializer&) override { return false; } // disable model saving

    std::vector<GameObject*> rootGameObjects;
    std::vector<UniPtr<GameObject>> gameObjects;
    std::vector<UniPtr<Mesh2>> meshes;
    std::vector<UniPtr<Texture>> textures;
    std::vector<UniPtr<Material>> materials;
};
} // namespace Engine
