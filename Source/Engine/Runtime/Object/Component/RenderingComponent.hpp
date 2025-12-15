#pragma once
#include "Engine/Runtime/Object/Component/Component.hpp"
#include "Engine/Runtime/System/SceneManager/RenderingObjectList.hpp"

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
class RenderingComponent : public RenderingComponentBase, public RenderingObject<T>
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

#define DECLARE_RENDERING_COMPONENT(TypeName) \
    DECLARE_OBJECT()                          \
public:                                       \
    TypeName() : TypeName(nullptr) {}         \
    const std::string& GetName() const override;    \
    TypeName(GameObject* gameObject) : RenderingComponent<TypeName>(gameObject) {}

#define DECLARE_RENDERING_COMPONENT_CONSTRUCT(TypeName) \
    DECLARE_OBJECT()                                    \
public:                                                 \
    TypeName() : TypeName(nullptr) {}                   \
    const std::string& GetName() const override;              \
    TypeName(GameObject* gameObject);

#define DEFINE_RENDERING_COMPONENT(TypeName, UUID) \
    DEFINE_OBJECT(TypeName, UUID)                  \
    const std::string& TypeName::GetName() const   \
    {                                              \
        static std::string name = #TypeName;       \
        return name;                               \
    }

#define DEFINE_RENDERING_COMPONENT_CONSTRUCT(TypeName, UUID) \
    DEFINE_OBJECT(TypeName, UUID)                            \
    const std::string& TypeName::GetName() const             \
    {                                                        \
        static std::string name = #TypeName;                 \
        return name;                                         \
    }                                                        \
    TypeName::TypeName(GameObject* gameObject) : RenderingComponent<TypeName>(gameObject)
