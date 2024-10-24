#pragma once
#include "Core/Asset.hpp"
#include "Core/Gizmo.hpp"
#include "Libs/Ptr.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
class GameObject;
class Scene;
class Component : public Object, public Serializable
{
public:
    Component(GameObject* gameObject);
    virtual ~Component();
    virtual void Tick() {};
    virtual void PrePhysicsTick() {};

    virtual const std::string& GetName() = 0;
    virtual std::unique_ptr<Component> Clone(GameObject& owner) = 0;
    GameObject* GetGameObject();

    bool IsEnabled()
    {
        return enabled;
    }

    void Enable()
    {
        if (enabled == false)
        {
            enabled = true;
            EnableImple();
        }
    };

    void Disable()
    {
        if (enabled == true)
        {
            enabled = false;
            DisableImple();
        }
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    Scene* GetScene();

    // called when play mode enter
    virtual void Awake() {}
    virtual void OnDrawGizmos() {};

protected:
    bool enabled = false;
    GameObject* gameObject;

    virtual void EnableImple() {};
    virtual void DisableImple() {};

    // editor only
    virtual void TransformChanged() {}

    friend class GameObject;
};
