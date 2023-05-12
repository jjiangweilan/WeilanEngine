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
class GameScene;
class GameObject : public Resource
{
public:
    GameObject(RefPtr<GameScene> gameScene);
    GameObject(GameObject&& other);
    GameObject();
    ~GameObject();
    template <class T, class... Args>
    T* AddComponent(Args&&... args);

    template <class T>
    T* GetComponent();

    RefPtr<Component> GetComponent(const char* name);

    std::vector<UniPtr<Component>>& GetComponents();
    void SetGameScene(RefPtr<GameScene> scene) { gameScene = scene; }
    RefPtr<GameScene> GetGameScene();
    RefPtr<Transform> GetTransform();
    void Tick();

private:
    std::vector<UniPtr<Component>> components;
    RefPtr<Transform> transform = nullptr;
    RefPtr<GameScene> gameScene = nullptr;

    friend class SerializableField<GameObject>;
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

template <>
struct SerializableField<GameObject>
{
    static void Serialize(GameObject* v, Serializer* s)
    {
        SerializableField<Resource>::Serialize(v, s);
        s->Serialize(v->components);
        s->Serialize(v->transform);
        s->Serialize(v->gameScene);
    }
    static void Deserialize(GameObject* v, Serializer* s)
    {
        SerializableField<Resource>::Deserialize(v, s);
        s->Deserialize(v->components);
        s->Deserialize(v->transform);
        s->Deserialize(v->gameScene);
    }
};

} // namespace Engine
