#pragma once
#include "MiddleLayer/EngineCommand/EngineCommand.hpp"

namespace Editor
{
class EngineCommandGUI
{
public:
    void EditorDraw();

private:
    void DrawCommandInput();
    void RemoveBlanks(EngineCommand::CommandList& list);

    bool showCommandInput = false;

    std::string inputBuffer = "";
    EngineCommand::CommandList commandListCache = {};
};
} // namespace Editor
