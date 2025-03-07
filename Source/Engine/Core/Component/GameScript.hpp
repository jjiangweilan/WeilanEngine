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

    void LuaOnStart();
    void LuaOnStop();
    void Tick() override;
    void OnStart() override;
    void OnStop() override;
    void OnDestroy() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;
    void LuaSerialize(Serializer* s) const;
    void LuaDeserialize(Serializer* s);
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    int AddOne(int x) { return 1 + x; }
    int LuaPushReferenceToStack();
    const std::string& GetLuaClassName() const { return luaClassName; }

private:
    ObjPtr<LuaScript> luaScript;
    using LuaRef = int;
    UUID luaBackendUUID;
    std::vector<std::string> serializationValKeys;
    std::string luaClassName = "";

    LuaRef luaRef = LUA_REFNIL;
};
