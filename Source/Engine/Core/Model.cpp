#include "Model.hpp"

namespace Engine
{
    Model::Model(std::unordered_map<std::string, UniPtr<Mesh>>&& meshes_, const UUID& uuid) : AssetObject(uuid), meshes(std::move(meshes_))
    {
        for(auto& iter : meshes)
        {
            meshNames.push_back(iter.first);
        }

        SERIALIZE_MEMBER(meshes);
    }

    RefPtr<Mesh> Model::GetMesh(const std::string& name)
    {
        auto iter = meshes.find(name);
        if (iter != meshes.end())
        {
            return iter->second;
        }

        return nullptr;
    }
}
