#pragma once

#include <filesystem>
#include "Libs/Ptr.hpp"
#include "Editor/EditorContext.hpp"

namespace Engine::Editor
{
    class Inspector
    {
        public:
            Inspector(RefPtr<AssetObject> target) : target(target){};
            virtual ~Inspector() {}
            virtual void Tick(RefPtr<EditorContext> editorContext) = 0;

        protected:
            RefPtr<Object> target;
    };
}
