#include "Core/GameObject.hpp"
#include <filesystem>
namespace Editor
{
class PrototypeUtils
{
public:
    static void MakePrototype(GameObject* go, const std::filesystem::path& path);

    // std::unique_ptr<GameObject> go = std::make_unique<GameObject>(*sceneTreeContextObject);
    // GameObject* prototype = (GameObject*)AssetDatabase::Singleton()->SaveAsset(std::move(go), go->GetName());
    // sceneTreeContextObject->SetPrototype(prototype);
};

} // namespace Editor
