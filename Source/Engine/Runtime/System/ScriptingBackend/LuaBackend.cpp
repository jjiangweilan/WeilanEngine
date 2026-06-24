#include "LuaBackend.hpp"

#include <array>
#include <filesystem>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace
{
std::filesystem::path GetExecutableDirectory()
{
#if defined(_WIN32)
    std::array<char, MAX_PATH> path{};
    DWORD size = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size != 0 && size < path.size())
    {
        return std::filesystem::path(path.data()).parent_path();
    }
#endif

    return std::filesystem::current_path();
}
} // namespace

LuaBackend::LuaBackend() {}

LuaBackend::~LuaBackend() {}

// int luaopen_WeilanEngine(lua_State* L)
// {
//     LuaBindings().BindClasses(L);
//
//     return 1;
// }

void LuaBackend::Init(const char* projectAssetFolder)
{
    if (instance == nullptr)
    {
        L = luaL_newstate();
        if (L == nullptr)
        {
            SPDLOG_ERROR("Failed to create Lua state");
            return;
        }
        luaL_openlibs(L);

        // set search path
        std::string executableDir = GetExecutableDirectory().generic_string();
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "path");
        const char* defaultPath = lua_tostring(L, -1);
        std::string searchPath = fmt::format(
            "?.lua;{}/?.lua;{}/share/lua/5.1/?.lua;{}/share/lua/5.1/?/init.lua;{}",
            std::string(projectAssetFolder),
            executableDir,
            executableDir,
            defaultPath != nullptr ? defaultPath : ""
        );
        lua_pop(L, 1);
        lua_pushstring(L, searchPath.c_str());
        lua_setfield(L, -2, "path");

        lua_getfield(L, -1, "cpath");
        const char* defaultCPath = lua_tostring(L, -1);
        std::string cSearchPath = fmt::format(
            "{}/lib/lua/5.1/?.dll;{}",
            executableDir,
            defaultCPath != nullptr ? defaultCPath : ""
        );
        lua_pop(L, 1);
        lua_pushstring(L, cSearchPath.c_str());
        lua_setfield(L, -2, "cpath");
        lua_pop(L, 1);

        // redirect print
        lua_getglobal(L, "_G");
        lua_pushcfunction(L, EnginePrint);
        lua_setfield(L, -2, "print");
        lua_pop(L, 1);

        LuaBindings().BindClasses(L);
        instance = this;
        currentStateUUID = UUID();
    }
}

int LuaBackend::EnginePrint(lua_State* L)
{
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; i++)
    {
        const char* value = lua_tostring(L, i);
        spdlog::info(value != nullptr ? value : luaL_typename(L, i));
    }

    return 0;
}

lua_State* LuaBackend::L = nullptr;

void LuaBackend::Destroy()
{
    if (L)
    {
        lua_close(L);
        L = nullptr;
    }

    if (instance == this)
    {
        instance = nullptr;
    }
    LuaTypeRegistery::typeToName.clear();
}

LuaBackend* LuaBackend::instance = nullptr;
UUID LuaBackend::currentStateUUID = UUID();
