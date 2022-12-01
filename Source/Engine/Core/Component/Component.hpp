#pragma once
#include "Code/Ptr.hpp"
#include "Core/AssetObject.hpp"
#include <string_view>
#include <string>
#include <unordered_map>
#include <functional>
namespace Engine
{
    template<class T>
    struct Duplicable
    {
        static constexpr int value = 0;
    };

    #define DUPLICABLE(xx) template<> struct Duplicable<xx>{ static constexpr int value = 1; };

    class GameObject;
    class Component : public AssetObject
    {
        DECLARE_ENGINE_OBJECT();
        public:
            Component() = default;
            Component(std::string_view className, RefPtr<GameObject> gameObject);
            virtual ~Component() = 0;
            virtual void Tick() {};
            RefPtr<GameObject> GetGameObject();

            // component name is used as an id when gameObject looks for component by name
            void SetName(const std::string& name) final {Object::SetName(name);}
            const std::string& GetName();
            static UniPtr<Component> CreateDerived(const std::string& className);
        protected:
            RefPtr<GameObject> gameObject;

        private:
            EDITABLE(std::string, className);

            friend class GameObject;
    };

    class ComponentRegister
    {
        public:
            static RefPtr<ComponentRegister> Instance();

            UniPtr<Component> CreateComponent(std::string_view name)
            {
                auto iter = registeredComponents.find(std::string(name));
                assert(iter != registeredComponents.end());
                return iter->second();
            }

            template<class T>

            bool Register(std::string_view name)
            {
                auto iter = registeredComponents.find(std::string(name));
                if (iter != registeredComponents.end())
                {
                    SPDLOG_WARN("component type is already registered %s", std::string(name).c_str());
                    return false;
                }

                registeredComponents[std::string(name)] = []() { return MakeUnique<T>(); };
                return true;
            }
        private:
            ComponentRegister();
            static UniPtr<ComponentRegister> instance;
            static UniPtr<ComponentRegister> CreateInstance();
            std::unordered_map<std::string, std::function<UniPtr<Component>()>> registeredComponents;
    };

    #define DECLARE_COMPONENT(Type) \
        DECLARE_ENGINE_OBJECT(); \
        private: \
        inline static const bool _Register_Component = ComponentRegister::Instance()->Register<Type>(#Type);
}
