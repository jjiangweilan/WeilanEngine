#pragma once
#include "Core/Asset.hpp"
#include "Core/Gizmo.hpp"
#include "Core/Ptr.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
class GameObject;
class Scene;
class Component : public Object
{
public:
    Component(GameObject* gameObject);
    virtual ~Component();
    virtual void Tick() {};
    virtual void PrePhysicsTick() {};

    virtual const std::string& GetName() = 0;
    virtual std::unique_ptr<Component> Clone(GameObject& owner) = 0;
    GameObject* GetGameObject();

    bool IsEnabled() { return enabled; }

    void Enable()
    {
        if (enabled == false)
        {
            enabled = true;
            OnEnable();
        }
    };

    void Disable()
    {
        if (enabled == true)
        {
            enabled = false;
            OnDisable();
        }
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    Scene* GetScene();

    // called when play mode enter
    virtual void OnStart() {}
    virtual void OnStop() {}
    virtual void OnDrawGizmos() {}
    virtual void OnLoaded() {}

protected:
    bool enabled = false;
    GameObject* gameObject;

    virtual void OnEnable() {};
    virtual void OnDisable() {};
    virtual void OnDestroy() {};

    // editor only
    virtual void TransformChanged() {}

    friend class GameObject;
};

/**
class ExampleComponent : public Component
{
    DECLARE_OBJECT();

public:
    ExampleComponent();
    ExampleComponent(GameObject* gameObject);
    ~ExampleComponent();

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void OnLoaded() override;
};
*/
