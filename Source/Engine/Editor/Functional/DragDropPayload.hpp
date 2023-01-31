#pragma once
#include "Core/Object.hpp"
#include "Libs/Ptr.hpp"
namespace Engine
{
enum class PayloadType
{
    Mesh,
};
struct DragDropPayload
{
    void* data;
    PayloadType type;
};
} // namespace Engine