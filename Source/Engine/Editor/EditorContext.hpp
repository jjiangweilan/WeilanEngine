#pragma once

#include "Core/AssetObject.hpp"
#include "Core/GameObject.hpp"

namespace Engine::Editor
{
template <class T>
class ContextObjectCarrier
{
public:
    RefPtr<T> carriedObject;
};

struct EditorContext
{
    RefPtr<Object> currentSelected = nullptr;
};
} // namespace Engine::Editor
