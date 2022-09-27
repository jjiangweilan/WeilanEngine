#pragma once

#include <filesystem>
#include "Code/Ptr.hpp"
#include "Editor/EditorContext.hpp"

namespace Engine::Editor
{
    class CustomExplorer
    {
        public:
            CustomExplorer(RefPtr<Object> target) : target(target){}
            virtual ~CustomExplorer() {}
            virtual void Tick(RefPtr<EditorContext> editorContext, const std::filesystem::path& path) = 0;
        protected:
            RefPtr<Object> target;
    };
}
