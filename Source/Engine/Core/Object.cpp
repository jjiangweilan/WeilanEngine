#include "Object.hpp"

namespace Engine
{
    void Object::SetName(const std::string& name)
    {
        this->name = name;
    }

    void Object::SetName(const char* name)
    {
        this->name = name;
    }

    const std::string& Object::GetName()
    {
        return name;
    }
}
