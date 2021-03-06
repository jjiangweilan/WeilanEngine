#include "Component/LuaScriptBridge.hpp"
#include "System/LuaScriptSystem.hpp"
#include "Manager/EngineManager.hpp"
#include "Component/LuaScript.hpp"
#include "GameObject/Entity.hpp"
namespace KuangyeEngine
{
SYSTEM_DEFINATION(LuaScriptSystem);

LuaScriptSystem::LuaScriptSystem()
{
}

void LuaScriptSystem::update()
{
    auto currentScene = EngineManager::GetKuangyeEngine()->getCurrentScene();
    for (auto &luaScript : LuaScript::collection)
    {
        if (luaScript->entity->IsEnable() || luaScript->entity->GetScene() != currentScene)
            lua_getglobal(luaScript->state, "update");
        if (lua_pcall(luaScript->state, 0, 0, 0))
        {
            std::cerr << luaScript->entity->name << ", error: " << lua_tostring(luaScript->state, -1) << std::endl;
        }
    }
}
} // namespace KuangyeEngine