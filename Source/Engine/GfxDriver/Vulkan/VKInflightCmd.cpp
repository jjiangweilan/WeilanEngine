#include "VKInflightCmd.hpp"

namespace Gfx
{

void VKFramePrepareData::AppendVKCommandBuffer(VKCommandBuffer* cmd)
{
    cmds.insert(cmds.end(), std::move_iterator(cmd->cmds.begin()), std::move_iterator(cmd->cmds.end()));
    readbacks = std::move(cmd->readbacks);

    cmd->readbacks.clear();
    cmd->cmds.clear();
}

void VKFramePrepareData::Clear()
{
    cmds.clear();
    readbacks.clear();
}
} // namespace Gfx
