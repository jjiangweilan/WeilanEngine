#pragma once
#include "../CustomExplorer.hpp"

namespace Engine
{
class Model;
}
namespace Engine::Editor
{
class ModelExplorer : public CustomExplorer
{
public:
    ModelExplorer(RefPtr<Object> target) : CustomExplorer(target){};
    virtual void Tick(RefPtr<EditorContext> editorContext, const std::filesystem::path& path) override;
};
} // namespace Engine::Editor
