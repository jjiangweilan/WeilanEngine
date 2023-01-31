#pragma once

#include "AssetObject.hpp"
#include "Core/Graphics/Mesh.hpp"
namespace Engine
{
class Model : public AssetObject
{
public:
    Model(std::unordered_map<std::string, UniPtr<Mesh>>&& meshes, const UUID& uuid);
    ~Model() override {}

    RefPtr<Mesh> GetMesh(const std::string& name);
    const std::vector<std::string>& GetMeshNames() const { return meshNames; }

private:
    bool Serialize(AssetSerializer&) override { return false; } // disable model saving

    std::unordered_map<std::string, UniPtr<Mesh>> meshes;
    std::vector<std::string> meshNames;
};
} // namespace Engine
