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
    Model2(std::vector<UniPtr<Mesh2>> meshes_, UUID uuid = UUID::empty) : AssetObject(uuid), meshes(std::move(meshes_))
    {
        SERIALIZE_MEMBER(meshes);
    };
    struct Group
    {
        Mesh2* mesh;
        Material* material;
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

    std::span<UniPtr<Mesh2>> GetMeshes() { return meshes; }

private:
    bool Serialize(AssetSerializer&) override { return false; } // disable model saving

    std::vector<UniPtr<Mesh2>> meshes;
};
} // namespace Engine
