#pragma once
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Core/Component/LuaScript.hpp"
#include "Script/LuaBackend.hpp"

using namespace Engine;
#define L LuaBackend::Instance()->GetL()

TEST(LuaScript, RefLuaClass) {
    LuaScript luaScript;

    // read file
    std::fstream f;
    std::filesystem::path filePath(__FILE__);
    filePath = filePath.parent_path() / "Resources/LuaScript.lua";
    f.open(filePath);
    std::stringstream ss;
    ss << f.rdbuf();
    luaL_dostring(L, ss.str().c_str());
    luaL_openlibs(L);

    luaScript.RefLuaClass("ScriptTest");

    lua_getglobal(L, "GlobalReadbackTest");
    if (lua_isstring(L, -1))
    {
        std::string readback = luaL_checkstring(L, -1);
        EXPECT_EQ(readback, "This should be read in host");
    }
    else
        FAIL();
}
