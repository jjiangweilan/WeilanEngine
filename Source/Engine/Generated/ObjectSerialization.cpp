#include "Core/GameObject.hpp"
#include "ObjectSerialization.hpp"
#include "Libs/RTTI.hpp"

void RegisterSerializedObjects()
{
    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, isPrototype);
    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, position);
    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, scale);
    REGISTER_RTTI_MEMBER_VARIABLE(GameObject, rotation);
}
