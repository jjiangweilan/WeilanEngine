#pragma once
#include "Libs/Ptr.hpp"
#include <glm/glm.hpp>
namespace Engine
{
class Input
{
public:
    static RefPtr<Input> Instance();

private:
    static UniPtr<Input> instance;
};
} // namespace Engine
