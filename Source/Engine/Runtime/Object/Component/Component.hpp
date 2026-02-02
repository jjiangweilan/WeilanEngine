#pragma once
#include "Editor/Gizmos/Gizmo.hpp"
#include "Editor/Gizmos/GizmoManager.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Core/Ptr.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
class GameObject;
class Scene;
class Component : public Object
{
    DECLARE_OBJECT();

public:
    Component(GameObject* gameObject);
    virtual ~Component();
    virtual void Tick() {}
    virtual void IdleTick() {}
    virtual void PrePhysicsTick() {};
    virtual void DebugDraw() {};

    virtual const std::string& GetName() const = 0;
    virtual std::unique_ptr<Component> Clone(GameObject& owner) { return nullptr; }
    GameObject* GetGameObject();

    bool IsEnabled() { return enabled; }
    bool IsActiveInScene();

    void Enable();
    void Disable();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    Scene* GetScene();

    void Init()
    {
        if (!isInited)
        {
            OnInit();
            isInited = true;
        }
    }

    void Destroy()
    {
        if (isAlive)
        {
            OnDestroy();
            isAlive = false;
        }
    }

    void SetGameObject(GameObject* go)
    {
        gameObject = go;
    }

    void Awake()
    {
        if (!isAwake)
        {
            OnAwake();
            isAwake = true;
        }
    }

    void Start()
    {
        if (!isStarted)
        {
            OnStart();
            isStarted = true;
        }
    }

    virtual void OnDrawGizmos() {}
    virtual void OnDrawGizmos(GizmoManager& gizmoContext) {}
    virtual void OnLoaded() {}

protected:
    bool isInited = false;
    bool isAlive = true;
    bool enabled = false;
    bool isAwake = false;
    bool isStarted = false;
    GameObject* gameObject;

    virtual void OnInit() {};
    virtual void OnDestroy() {};

    virtual void OnEnable() {};
    virtual void OnDisable() {};

    // called when play mode enter
    virtual void OnAwake() {}
    virtual void OnStart() {}

    // TODO: remove OnStop, merge it with OnDestroy
    virtual void OnStop() {}

    // editor only
    virtual void TransformChanged() {}

    friend class GameObject;
};

#define DECLARE_COMPONENT(TypeName)                             \
    DECLARE_OBJECT()                                            \
public:                                                         \
    TypeName() : Component(nullptr) {}                          \
    TypeName(GameObject* gameObject) : Component(gameObject) {} \
    const std::string& GetName() const override;                \
                                                                \
private:

#define DEFINE_COMPONENT(TypeName, UUID)         \
    DEFINE_OBJECT(Component, TypeName, UUID)     \
    const std::string& TypeName::GetName() const \
    {                                            \
        static std::string name = #TypeName;     \
        return name;                             \
    }

#define DECLARE_COMPONENT_CONSTRUCT(TypeName)    \
    DECLARE_OBJECT()                             \
public:                                          \
    TypeName() : TypeName(nullptr){};            \
    TypeName(GameObject* gameObject);            \
    const std::string& GetName() const override; \
                                                 \
private:

#define DEFINE_COMPONENT_CONSTRUCT(TypeName, UUID) \
    DEFINE_OBJECT(Component, TypeName, UUID)       \
    const std::string& TypeName::GetName() const   \
    {                                              \
        static std::string name = #TypeName;       \
        return name;                               \
    }                                              \
    TypeName::TypeName(GameObject* gameObject) : Component(gameObject)

/**
class ExampleComponent : public Component
{
    DECLARE_OBJECT();

public:
    ExampleComponent();
    ExampleComponent(GameObject* gameObject);
    ~ExampleComponent();
    const std::string& GetName() const override;

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnLoaded() override;
};
*/
