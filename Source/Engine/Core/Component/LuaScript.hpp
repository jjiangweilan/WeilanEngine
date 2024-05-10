#pragma once
#include "Component.hpp"
#include "ThirdParty/lua/lua.hpp"

class LuaScript : public Component
{
    DECLARE_OBJECT();

public:
    LuaScript();
    LuaScript(GameObject* gameObject);

    const std::string& GetLuaClass()
    {
        return luaClassName;
    }
    void RefLuaClass(const char* luaClass);
    void Construct();
    void Destruct();
    void Tick() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override
    {
        return nullptr;
    }

    const std::string& GetName() override;

private:
    std::string luaClassName;

    using LuaRef = int;

    LuaRef luaRef = LUA_REFNIL;
    LuaRef luaRefConstruct = LUA_REFNIL;
    LuaRef luaRefDestruct = LUA_REFNIL;
    LuaRef luaRefTick = LUA_REFNIL;
};
