#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/CommandQueue.hpp"
#include <vector>

namespace Rendering
{
class CmdSubmitGroup
{
public:
    CmdSubmitGroup(){};

    void AddCmd(Gfx::CommandBuffer* cmd)
    {
        cmds.push_back(cmd);
    };

    void InsertToFront(CmdSubmitGroup&& other)
    {
        if (!other.cmds.empty())
        {
            cmds.insert(cmds.begin(), other.cmds.begin(), other.cmds.end());
            other.Clear();
        }
    }

    void AppendToBack(CmdSubmitGroup&& other)
    {
        if (!other.cmds.empty())
        {
            cmds.insert(cmds.end(), other.cmds.begin(), other.cmds.end());
            other.Clear();
        }
    }

    std::span<Gfx::CommandBuffer*> GetCmds()
    {
        return cmds;
    }
    void Clear()
    {
        cmds.clear();
    }

private:
    std::vector<Gfx::CommandBuffer*> cmds;
};
} // namespace Rendering
