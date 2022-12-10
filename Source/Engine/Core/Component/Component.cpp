#include "Component.hpp"

namespace Engine
{
    Component::Component()
    {
        SERIALIZE_MEMBER(className);
    }

    Component::Component(std::string_view className, RefPtr<GameObject> gameObject) : Component()
    {
        this->gameObject = gameObject;
        this->className = className;
    }

    Component::~Component(){}

    RefPtr<GameObject> Component::GetGameObject()
    {
        return gameObject;
    }

    const std::string& Component::GetName()
    {
        return className;
    }

    UniPtr<Component> Component::CreateDerived(const std::string& className)
    {
        return ComponentRegister::Instance()->CreateComponent(className);
    }

    ComponentRegister::ComponentRegister(){}

    UniPtr<ComponentRegister> ComponentRegister::CreateInstance()
    {
        if (instance == nullptr)
        {
            return UniPtr<ComponentRegister>(new ComponentRegister());
        }
        else return std::move(instance);
    }

    UniPtr<ComponentRegister> ComponentRegister::instance = CreateInstance();

    RefPtr<ComponentRegister> ComponentRegister::Instance()
    {
        if (instance == nullptr)
        {
            instance = UniPtr<ComponentRegister>(new ComponentRegister());
        }

        return instance;
    }
}
