#pragma once

#include "Asset.hpp"
#include "Component/Transform.hpp"
#include "Libs/Ptr.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>
namespace Engine
{
class Scene;
class GameObject : public Asset
{
    DECLARE_ASSET();

public:
    GameObject(Scene* gameScene);
    GameObject(GameObject&& other);
    GameObject(const GameObject& other);
    GameObject();
    ~GameObject();
    template <class T, class... Args>
    T* AddComponent(Args&&... args);

    template <class T>
    T* GetComponent();

    Component* GetComponent(const char* name); // obsolete

    std::vector<std::unique_ptr<Component>>& GetComponents();
    void SetGameScene(Scene* scene)
    {
        gameScene = scene;
    }
    Scene* GetGameScene();
    Transform* GetTransform();
    void Tick();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    std::vector<std::unique_ptr<Component>> components;
    Transform* transform = nullptr;
    Scene* gameScene = nullptr;
};

template <class T, class... Args>
T* GameObject::AddComponent(Args&&... args)
{
    auto p = std::make_unique<T>(this, args...);
    T* temp = p.get();
    components.push_back(std::move(p));
    return temp;
}

template <class T>
T* GameObject::GetComponent()
{
    for (auto& p : components)
    {
        T* cast = dynamic_cast<T*>(p.get());
        if (cast != nullptr)
            return cast;
    }
    return nullptr;
}

} // namespace Engine
