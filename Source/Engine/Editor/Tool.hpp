#pragma once
#include "Rendering/CmdSubmitGroup.hpp"
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Engine::Editor
{

// A Tool is a class that will have a `MenuItem`
// once clicked it will open a window to interact with
class Tool
{
public:
    virtual ~Tool() {}
    // return a vector of string, the last string will be the menuitem, other will be menu
    virtual std::vector<std::string> GetToolMenuItem() = 0;

    // all the cmds are executed before GameEditor executes its main render commands
    virtual Rendering::CmdSubmitGroup GetCmdSubmitGroup()
    {
        return Rendering::CmdSubmitGroup();
    };

    // return false if the tool want to close
    virtual bool Tick() = 0;
};
} // namespace Engine::Editor
