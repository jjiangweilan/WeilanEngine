#include "LuaImporter.hpp"
#include "Script/LuaBackend.hpp"

namespace Engine::Internal
{
    UniPtr<AssetObject> LuaImporter::Import(const std::filesystem::path& path, ReferenceResolver& refResolver, const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs)
    {
        LuaBackend::Instance()->LoadFile(path);
        return nullptr;
    }
}
