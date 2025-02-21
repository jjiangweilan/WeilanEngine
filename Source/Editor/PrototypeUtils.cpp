#include "PrototypeUtils.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"

namespace Editor
{
void PrototypeUtils::MakePrototype(GameObject* go, const std::filesystem::path& path)
{
    //if (go == nullptr)
    //    return;
    //Scene* scene = go->GetScene();
    //if (scene == nullptr)
    //    return;

    //auto uniqueGO = scene->RetrieveGameObject(go);
    //AssetDatabase::Singleton()->SaveAsset(std::move(uniqueGO), path);
}
} // namespace Editor
