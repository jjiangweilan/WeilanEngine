#pragma once
#include "Core/Component/Component.hpp"
#include "Core/Scene/RenderingObjectList.hpp"

class RenderingComponentBase : public Component
{
public:
    RenderingComponentBase() : Component(nullptr) {}
    RenderingComponentBase(GameObject* go) : Component(go) {}

protected:
    void AddToRenderingScene(uint32_t objectTypeID, RenderingObjectBase* self);
    void RemoveFromRenderingScene(uint32_t objectTypeID);

private:
    RenderingObjectList::ObjectIndex renderingObjectID = -1;
};

template <class T>
class RenderingComponent : public RenderingComponentBase, RenderingObject<T>
{
public:
    RenderingComponent() : RenderingComponentBase(nullptr) {}
    RenderingComponent(GameObject* go) : RenderingComponentBase(go) {}

    void OnEnable() override
    {
        Component::OnEnable();
        AddToRenderingScene(RenderingObject<T>::renderObjectTypeID, this);
    }

    void OnDisable() override
    {
        Component::OnDisable();
        RemoveFromRenderingScene(RenderingObject<T>::renderObjectTypeID);
    }
};
