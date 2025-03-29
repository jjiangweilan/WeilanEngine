#include "VKInflightCmd.hpp"

namespace Gfx
{

void VKFramePrepareData::AppendVKCommandBuffer(VKCommandBuffer* cmd)
{
    cmds.insert(cmds.end(), std::move_iterator(cmd->cmds.begin()), std::move_iterator(cmd->cmds.end()));
    memory.push_back(std::move(cmd->tmpMemory));
    readbacks = std::move(cmd->readbacks);

    cmd->readbacks.clear();
    cmd->tmpMemory.Reset();
    cmd->cmds.clear();
}

void VKFramePrepareData::Clear()
{
    cmds.clear();
    readbacks.clear();
    for (auto& m : memory)
        m.Reset();
    memory.clear();
}
} // namespace Gfx
