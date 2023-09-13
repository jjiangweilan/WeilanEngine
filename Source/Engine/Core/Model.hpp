#pragma once

#include "Core/Graphics/Mesh.hpp"
#include "Resource.hpp"
namespace Engine
{
class Model : public Resource
{
    DECLARE_RESOURCE();

public:
    Model() {}
    Model(std::unordered_map<std::string, UniPtr<Mesh>>&& meshes, const UUID& uuid);
    ~Model() override {}

    RefPtr<Mesh> GetMesh(const std::string& name);
    const std::vector<std::string>& GetMeshNames() const
    {
        return meshNames;
    }

private:
    std::unordered_map<std::string, UniPtr<Mesh>> meshes;
    std::vector<std::string> meshNames;
};
} // namespace Engine
