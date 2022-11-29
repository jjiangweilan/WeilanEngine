#pragma once

#include "ThirdParty/lua/lua.hpp"
#include "Code/Ptr.hpp"
#include <filesystem>
namespace Engine
{
    class LuaBackend
    {
        public:
            static RefPtr<LuaBackend> Instance();

            inline lua_State* GetL() { return state; }
            void LoadFile(const std::filesystem::path& path);
        private:
            LuaBackend();

            lua_State* state;

            static UniPtr<LuaBackend> instance;
    };
}
