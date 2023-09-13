#include "Model.hpp"

namespace Engine
{
DEFINE_RESOURCE(Model, "168A5885-3B44-44F3-A29F-EC9CDECE6F67");

Model::Model(std::unordered_map<std::string, UniPtr<Mesh>>&& meshes_, const UUID& uuid) : meshes(std::move(meshes_))
{
    SetUUID(uuid);
    for (auto& iter : meshes)
    {
        meshNames.push_back(iter.first);
    }
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

} // namespace Engine
