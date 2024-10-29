#include "Core/GameObject.hpp"
#include <filesystem>
namespace Editor
{
class PrototypeUtils
{
public:
    static void MakePrototype(GameObject* go, const std::filesystem::path& path);
};

} // namespace Editor
