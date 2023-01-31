#pragma once

#include "Editor/EditorContext.hpp"
#include "Libs/Ptr.hpp"
#include <filesystem>

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
} // namespace Engine::Editor
