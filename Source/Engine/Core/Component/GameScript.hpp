#pragma once
#include "Component.hpp"
#include "Core/LuaScript.hpp"
#include "ThirdParty/lua/lua.hpp"

class GameScript : public Component
{
    DECLARE_OBJECT();

public:
    GameScript();
    GameScript(GameObject* gameObject);

    ObjPtr<LuaScript> GetScript() { return luaScript; }
    void SetScript(ObjPtr<LuaScript> luaScript);

    void Construct();
    void Destruct();
    void Tick() override;
    void OnDestroy() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override { return nullptr; }
    const std::string& GetName() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnLoaded() override;

    int AddOne(int x)
    {
        return 1 + x;
    }

private:
    ObjPtr<LuaScript> luaScript;
    using LuaRef = int;

    LuaRef luaRef = LUA_REFNIL;
};
