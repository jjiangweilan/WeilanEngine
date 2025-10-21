#pragma once
#include "Component.hpp"
#include "Core/LuaScript.hpp"
#include "ThirdParty/lua/lua.hpp"

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/ContactListener.h>

class PhysicsBody;
class GameScript : public Component
{
    DECLARE_OBJECT();

public:
    GameScript();
    GameScript(GameObject* gameObject);

    ObjPtr<LuaScript> GetScript() { return luaScript; }
    void SetScript(ObjPtr<LuaScript> luaScript);

    void ReloadScript();
    void LuaOnStart();
    void LuaOnStop();
    void Tick() override;
    void OnStart() override;
    void OnStop() override;
    void OnDestroy() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() const override;
    void LuaSerialize(Serializer* s) const;
    void LuaDeserialize(Serializer* s);
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    int LuaPushReferenceToStack();
    bool CallLua(const char* functionName);
    const std::string& GetLuaClassName() const { return luaClassName; }

private:
    ObjPtr<LuaScript> luaScript;
    using LuaRef = int;
    UUID luaBackendUUID;
    std::vector<std::string> serializationValKeys;
    std::string luaClassName = "";

    // lua state
    bool isScriptStarted = false;

    // game callback functions
    bool hasOnContactAdded = false;
    bool hasOnContactRemoved = false;
    bool hasOnContactPersisted = false;
    bool hasOnContactValidate = false;

    int onContactID_Added = -1;
    int onContactID_Removed = -1;

    LuaRef luaRef = LUA_REFNIL;

    void RegisterPhysicsCallbacks();
    void UnregisterPhysicsCallbacks();

    void OnContactAdded(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&);
    void OnContactRemoved(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&);

    void RemoveScript();

};
