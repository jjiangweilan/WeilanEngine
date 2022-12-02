#pragma once
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Core/Component/LuaScript.hpp"
#include "Core/GameObject.hpp"
#include "Script/LuaBackend.hpp"

using namespace Engine;
#define L LuaBackend::Instance()->GetL()

class LuaScript_F: public ::testing::Test { 
public: 
    inline static bool init = false;

    LuaScript_F( ) { 
        // read file
        if (!init)
        {
            std::fstream f;
            std::filesystem::path filePath(__FILE__);
            filePath = filePath.parent_path() / "Resources/LuaScript.lua";
            f.open(filePath);
            std::stringstream ss;
            ss << f.rdbuf();
            luaL_dostring(L, ss.str().c_str());
            init = true;
        }
    } 

    void SetUp( ) { 
    }

    void TearDown( ) { 
    }

    ~LuaScript_F( )  { 
    }
};

TEST_F(LuaScript_F, CallFromEngine)
{
    LuaScript luaScript;
    luaScript.RefLuaClass("ScriptTest");

    lua_getglobal(L, "GlobalReadbackTest");
    if (lua_isstring(L, -1))
    {
        std::string readback = luaL_checkstring(L, -1);
        EXPECT_EQ(readback, "This should be read in host");
    }
    else
    {
        FAIL();
        lua_pop(L, 1);
    }

}

TEST(LuaScript, CallToEngine)
{
    std::string testName = "TestName";
    GameObject go;
    go.GetTransform()->SetPosition({1,2,3});
    go.SetName(testName);
    LuaScript luaScript(&go);
    luaScript.RefLuaClass("ScriptTest");
    luaScript.Tick();
    EXPECT_EQ(go.GetTransform()->GetPosition(), glm::vec3(3,2,1));

    lua_getglobal(L, "GlobalReadbackTestTick");
    if (lua_isstring(L, -1))
    {
        std::string readback = luaL_checkstring(L, -1);
        EXPECT_EQ(readback, testName);
    }
    else
    {
        FAIL();
        lua_pop(L, 1);
    }

    lua_close(L);
}
