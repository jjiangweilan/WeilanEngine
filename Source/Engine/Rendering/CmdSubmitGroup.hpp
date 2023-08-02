#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/CommandQueue.hpp"
#include <vector>

namespace Engine::Rendering
{
class CmdSubmitGroup
{
public:
    CmdSubmitGroup(){};

    void AddCmd(Gfx::CommandBuffer* cmd)
    {
        cmds.push_back(cmd);
    };

    void InsertGroup(CmdSubmitGroup&& other)
    {
        if (!other.cmds.empty())
        {
            cmds.insert(cmds.begin(), other.cmds.begin(), other.cmds.end());
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
} // namespace Engine::Rendering
