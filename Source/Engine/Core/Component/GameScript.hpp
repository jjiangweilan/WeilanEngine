#pragma once
#include "Component.hpp"
#include "Core/LuaScript.hpp"
#include "ThirdParty/lua/lua.hpp"

#include <unordered_map>

class GameScript : public Component
{
    DECLARE_OBJECT();

public:
    // a list of lua supported type stored in c++
    using LuaSerializationClientSideCache = nlohmann::json;

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

    int AddOne(int x)
    {
        return 1 + x;
    }

private:
    ObjPtr<LuaScript> luaScript;
    using LuaRef = int;
    UUID luaBackendUUID;
    LuaSerializationClientSideCache luaDataCache;
    std::vector<std::string> serializationValKeys;

    LuaRef luaRef = LUA_REFNIL;
};
