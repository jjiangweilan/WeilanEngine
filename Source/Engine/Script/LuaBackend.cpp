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
            if (luaL_dostring(state, ss.str().c_str()) != 0)
            {
                const char* str = lua_tostring(state, -1);
                SPDLOG_ERROR("Con't load lua file {}", str);
            }
        }
        else
            SPDLOG_ERROR("Can't read lua file {}", path.string());
    }

    void LuaBackend::LoadLuaInFolder(const std::filesystem::path& folder)
    {
        LoadLuaInFolderIter(folder);
    }

    void LuaBackend::LoadLuaInFolderIter(const std::filesystem::path& entry)
    {
        if (std::filesystem::is_directory(entry))
        {
            for (auto const& dir_entry : std::filesystem::directory_iterator(entry)) 
            {
                LoadLuaInFolderIter(dir_entry);
            }
        }
        else
        {
            if (entry.extension() == ".lua")
            {
                LoadFile(entry);
            }
        }
    }

    UniPtr<LuaBackend> LuaBackend::instance = nullptr;
}
