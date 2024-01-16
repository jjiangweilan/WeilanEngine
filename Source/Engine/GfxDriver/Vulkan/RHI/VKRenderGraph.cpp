#include "VKRenderGraph.hpp"

namespace Gfx
{

void VKRenderGraph::Schedule(VKCommandBuffer2& cmd)
{
    auto cmds = cmd.GetCmds();

    for (int visitIndex = 0; visitIndex < cmds.size(); visitIndex++)
    {
        auto& cmd = cmds[visitIndex];
        switch (cmd.type)
        {
            case VKCmdType::BeginRenderPass: break; {
                }
            case VKCmdType::EndRenderPass: break;
        }
    }
}
} // namespace Gfx
