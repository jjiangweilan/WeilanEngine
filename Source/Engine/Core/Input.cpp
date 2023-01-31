#include "Input.hpp"

namespace Engine
{
RefPtr<Input> Input::Instance()
{
    if (instance == nullptr)
    {
        instance = MakeUnique<Input>();
    }

    return instance;
}

UniPtr<Input> Input::instance = nullptr;
} // namespace Engine
