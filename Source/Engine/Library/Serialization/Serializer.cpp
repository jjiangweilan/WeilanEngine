#include "Serializer.hpp"

void Serializer::Deserialize(std::string_view name, std::nullptr_t, const ReferenceResolveCallback& callback)
{
    UUID uuid;
    Deserialize(name, uuid);
    if (resolveCallbacks && uuid != UUID::GetEmptyUUID())
    {
        (*resolveCallbacks)[uuid].emplace_back(nullptr, uuid, callback);
    }
}
