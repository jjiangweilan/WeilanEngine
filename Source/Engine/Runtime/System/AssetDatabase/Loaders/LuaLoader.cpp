#include "LuaLoader.hpp"
#include "Engine/Game/LuaScript.hpp"
#include "../AssetDatabase.hpp"

DEFINE_ASSET_LOADER(LuaLoader, "lua")

const std::vector<std::type_index>& LuaLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(LuaScript)};
    return types;
}

void LuaLoader::Load()
{
    auto luaScript = std::make_unique<LuaScript>();

    auto scriptFilePath = std::filesystem::relative(absoluteAssetPath, AssetDatabase::Singleton()->GetAssetDirectory());
    auto scriptPath = scriptFilePath.replace_extension("").string(); 
    std::ranges::replace(scriptPath, '/', '.');
    std::ranges::replace(scriptPath, '\\', '.');

    luaScript->LoadScript(scriptPath.c_str());

    asset = std::move(luaScript);
}
