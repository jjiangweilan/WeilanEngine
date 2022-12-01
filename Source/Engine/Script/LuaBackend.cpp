#include "LuaBackend.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
namespace Engine
{
    LuaBackend::LuaBackend():
        state(luaL_newstate()),
        wraps(state)
    {
        luaL_openlibs(state);
    }

    RefPtr<LuaBackend> LuaBackend::Instance()
    {
        if (instance == nullptr)
        {
            instance = UniPtr(new LuaBackend());
        }

        return instance.Get();
    }

    void LuaBackend::LoadFile(const std::filesystem::path& path)
    {
        std::fstream f;
        f.open(path);

        if (f.is_open() && f.good())
        {
            std::stringstream ss;
            ss << f.rdbuf();
            luaL_dostring(state, ss.str().c_str());
        }
        else
            SPDLOG_ERROR("Can't load lua file {}", path.string());
    }

    UniPtr<LuaBackend> LuaBackend::instance = nullptr;
}
