#include "Core/GameObject.hpp"
#include "Core/Graphics/Material.hpp"
#include "Graphics/Mesh2.hpp"
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>

namespace Engine
{
class Model2 : public Resource
{
public:
    Model2(std::vector<GameObject*>&& rootGameObjects,
           std::vector<UniPtr<GameObject>>&& gameObjects,
           std::vector<UniPtr<Mesh2>>&& meshes,
           std::vector<UniPtr<Texture>> textures,
           std::vector<UniPtr<Material>> materials,
           UUID uuid = UUID::empty)
        : rootGameObjects(std::move(rootGameObjects)), gameObjects(std::move(gameObjects)),
          meshes(std::move(meshes)), textures(std::move(textures)), materials(std::move(materials))
    {
        SetUUID(uuid);
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
    std::vector<GameObject*> rootGameObjects;
    std::vector<UniPtr<GameObject>> gameObjects;
    std::vector<UniPtr<Mesh2>> meshes;
    std::vector<UniPtr<Texture>> textures;
    std::vector<UniPtr<Material>> materials;

    friend class SerializableField<Model2>;
};
// template<>
// struct SerializableField<Model2>
// {
//     static void Serialize(Model2* v, Serializer* s)
//     {
//         // s->Serialize(v->meshes);
//         // s->Serialize(v->textures);
//         // s->Serialize(v->materials);
//         // s->Serialize(v->gameObjects);
//         // s->Serialize(v->rootGameObjects);
//     }
// }
} // namespace Engine
