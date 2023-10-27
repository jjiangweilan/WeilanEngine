#include "FrameGraphEditor.hpp"
#include "ThirdParty/imgui/GraphEditor.h"

namespace Engine::Editor
{
void FrameGraphEditor::Draw()
{
    if (graph)
    {
        for (auto& node : graph->GetNodes())
        {}
    }
}
} // namespace Engine::Editor
