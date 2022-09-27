#pragma once

#include "Component/Transform.hpp"
#include "Code/Ptr.hpp"
#include "AssetObject.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <typeinfo>
#include <string>
namespace Engine
{
    class GameScene;
    class GameObject : public AssetObject
    {
        public:
            GameObject(RefPtr<GameScene> gameScene);
            GameObject(GameObject&& other);
            GameObject();
            ~GameObject();
            template<class T, class... Args>
            T* AddComponent(Args&&... args);

            template<class T>
            T* GetComponent();

            std::vector<UniPtr<Component>>& GetComponents();
            void SetGameScene(RefPtr<GameScene> scene) { gameScene = scene; }
            RefPtr<GameScene> GetGameScene();
            RefPtr<Transform> GetTransform();
            void Tick();

        private:

            EDITABLE(std::vector<UniPtr<Component>>, components);
            RefPtr<Transform> transform;
            RefPtr<GameScene> gameScene;

            void DeserializeInternal(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver) override;
    };

    template<class T, class... Args>
    T* GameObject::AddComponent(Args&&... args)
    {
        if (!Duplicable<T>::value)
        {
            for(auto& p : components)
            {
                if(dynamic_cast<T*>(p.Get()) != nullptr)
                    return nullptr;
            }
        }

        auto p = MakeUnique<T>(this, args...);
        T* temp = p.Get();
        components.push_back(std::move(p));
        return temp;
    }

    template<class T>
    T* GameObject::GetComponent()
    {
        for(auto& p : components)
        {
            T* cast = dynamic_cast<T*>(p.Get());
            if(cast != nullptr)
                return cast;
        }
        return nullptr;
    }
}
