#pragma once

#include "Component/Transform.hpp"
#include "Libs/Ptr.hpp"
#include "Resource.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>
namespace Engine
{
class Scene;
class GameObject : public Resource
{
public:
    GameObject(RefPtr<Scene> gameScene);
    GameObject(GameObject&& other);
    GameObject();
    ~GameObject();
    template <class T, class... Args>
    T* AddComponent(Args&&... args);

    template <class T>
    T* GetComponent();

    RefPtr<Component> GetComponent(const char* name);

    std::vector<UniPtr<Component>>& GetComponents();
    void SetGameScene(RefPtr<Scene> scene)
    {
        gameScene = scene;
    }
    RefPtr<Scene> GetGameScene();
    RefPtr<Transform> GetTransform();
    void Tick();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    std::vector<UniPtr<Component>> components;
    RefPtr<Transform> transform = nullptr;
    RefPtr<Scene> gameScene = nullptr;
};

template <class T, class... Args>
T* GameObject::AddComponent(Args&&... args)
{
    auto p = MakeUnique<T>(this, args...);
    T* temp = p.Get();
    components.push_back(std::move(p));
    return temp;
}

template <class T>
T* GameObject::GetComponent()
{
    for (auto& p : components)
    {
        T* cast = dynamic_cast<T*>(p.Get());
        if (cast != nullptr)
            return cast;
    }
    return nullptr;
}

} // namespace Engine
