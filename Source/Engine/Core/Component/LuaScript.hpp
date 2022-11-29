#pragma once
#include "Component.hpp"
#include "ThirdParty/lua/lua.hpp"

namespace Engine
{
    class LuaScript : public Component
    {
        DECLARE_COMPONENT(LuaScript);
        public:

        LuaScript();
        LuaScript(GameObject* gameObject);

        const std::string& GetLuaClass() {return luaClassName;}
        void RefLuaClass(const char* luaClass);
        void Construct();
        void Destruct();
        void Tick() override;

        private:
            EDITABLE(std::string, luaClassName);

            using LuaRef = int;

            LuaRef luaRef = LUA_REFNIL;
            LuaRef luaRefConstruct = LUA_REFNIL;
            LuaRef luaRefDestruct = LUA_REFNIL;
            LuaRef luaRefTick = LUA_REFNIL;
    };
}
