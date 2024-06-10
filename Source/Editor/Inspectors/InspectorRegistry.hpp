#pragma once
#include "Core/Object.hpp"
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace Editor
{
class InspectorBase;
class InspectorRegistry
{
public:
    using Registry = std::unordered_map<std::type_index, std::unique_ptr<InspectorBase>>;

    // this function automatically set inspector's target to obj
    static InspectorBase* GetInspector(Object& obj);
    template <class Inspector, class T>
    static char Register();

    static void DestroyAll();

private:
    static Registry& GetRegistry();
    static const char _defaultInspector;
};

template <class InspectorType, class T>
char InspectorRegistry::Register()
{
    GetRegistry()[typeid(T)] = std::unique_ptr<InspectorBase>(new InspectorType);
    return '1';
}
} // namespace Editor
