#pragma once

#include "ThirdParty/lua/lua.hpp"
#include "Code/Ptr.hpp"
#include "LuaWraps.hpp"
#include <filesystem>
namespace Engine
{
    class LuaBackend
    {
        public:
            static RefPtr<LuaBackend> Instance();

            inline lua_State* GetL() { return state; }
            void LoadFile(const std::filesystem::path& path);
            void OpenL() { if (!state) state = luaL_newstate(); }
            void CloseL() { if (state) lua_close(state); }
        private:
            LuaBackend();

            lua_State* state = nullptr;
            LuaWraps wraps;

            static UniPtr<LuaBackend> instance;
    };
}
