#include "VKInflightCmd.hpp"

namespace Gfx
{

void VKFramePrepareData::AppendVKCommandBuffer(VKCommandBuffer* cmd)
{
    cmds.insert(cmds.end(), std::move_iterator(cmd->cmds.begin()), std::move_iterator(cmd->cmds.end()));
    memory.Append(cmd->tmpMemory.GetMem(), cmd->tmpMemory.GetSize());
    cmd->tmpMemory.Reset();
    readbacks = std::move(cmd->readbacks);
}

void VKFramePrepareData::Clear()
{
    cmds.clear();
    memory.Reset();
    readbacks.clear();
}
} // namespace Gfx
