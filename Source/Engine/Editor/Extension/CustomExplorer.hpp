#pragma once

#include "Editor/EditorContext.hpp"
#include "Libs/Ptr.hpp"
#include <filesystem>

namespace Engine::Editor
{
class CustomExplorer
{
public:
    CustomExplorer(RefPtr<Object> target) : target(target) {}
    virtual ~CustomExplorer() {}
    virtual void Tick(RefPtr<EditorContext> editorContext, const std::filesystem::path& path) = 0;

protected:
    RefPtr<Object> target;
};
} // namespace Engine::Editor
