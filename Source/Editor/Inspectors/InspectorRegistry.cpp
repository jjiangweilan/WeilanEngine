#include "InspectorRegistry.hpp"
#include "Inspector.hpp"

namespace Editor
{

InspectorRegistry::Registry& InspectorRegistry::GetRegistry()
{
    static Registry registry;
    return registry;
}

void InspectorRegistry::DestroyAll()
{
    GetRegistry().clear();
}

InspectorBase* InspectorRegistry::GetInspector(Object& obj)
{
    Registry& re = GetRegistry();
    auto iter = re.find(typeid(obj));
    if (iter != re.end())
    {
        return iter->second.get();
    }

    auto defaultInspector = re.find(typeid(Object))->second.get();
    return defaultInspector;
}

const char InspectorRegistry::_defaultInspector = InspectorRegistry::Register<Inspector<Object>, Object>();
} // namespace Editor
