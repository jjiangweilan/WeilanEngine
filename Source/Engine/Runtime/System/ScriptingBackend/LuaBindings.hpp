#pragma once
#include "LuaBindings_Common.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaHeaders.hpp"

class LuaBindings
{
public:
    // this function will leave a table on stack
    void BindClasses(lua_State* L);
};

void BindGeneratedClasses(lua_State* L);
void ClearLuaCreatedRuntimeAssets();
