#pragma once

#include "Core/GameObject.hpp"
#include "Core/AssetObject.hpp"

namespace Engine::Editor
{
    template<class T>
    class ContextObjectCarrier
    {
        public:
            RefPtr<T> carriedObject;
    };

    struct EditorContext
    {
        RefPtr<Object> currentSelected = nullptr;
    };
}
