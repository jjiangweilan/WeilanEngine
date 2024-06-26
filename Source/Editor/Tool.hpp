#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Editor
{

// A Tool is a class that will have a `MenuItem`
// once clicked it will open a window to interact with
class Tool
{
public:
    virtual ~Tool() {}
    // return a vector of string, the last string will be the menuitem, other will be menu
    virtual std::vector<std::string> GetToolMenuItem() = 0;

    // return false if the tool want to close
    virtual bool Tick() = 0;

    // used if this tool needs to render something on GPU
    virtual void Render(Gfx::CommandBuffer& cmd) {};

    // called when the tool window is open
    virtual void Open() {};

    // called when the tool window is closed
    virtual void Close() {};
};
} // namespace Editor
