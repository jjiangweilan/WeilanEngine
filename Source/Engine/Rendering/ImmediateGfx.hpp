#pragma once
#include "GfxDriver/GfxDriver.hpp"

namespace Engine
{

template <class T>
concept OnetimeSubmitFunc = requires(T f, CommandBuffer* cmd) { f(*cmd); };

class ImmediateGfx
{
public:
    template <OnetimeSubmitFunc Func>
    static void OnetimeSubmit(Func f)
    {
        auto queue = GetGfxDriver()->GetQueue(QueueType::Main).Get();
        auto commandPool = GetGfxDriver()->CreateCommandPool({.queueFamilyIndex = queue->GetFamilyIndex()});
        std::unique_ptr<CommandBuffer> cmd = commandPool->AllocateCommandBuffers(CommandBufferType::Primary, 1)[0];
        cmd->Begin();
        f(*cmd);
        cmd->End();
        RefPtr<CommandBuffer> cmdBufs[] = {cmd.get()};
        GetGfxDriver()->QueueSubmit(queue, cmdBufs, {}, {}, {}, nullptr);
        GetGfxDriver()->WaitForIdle();
        cmd = nullptr;
        commandPool = nullptr;
    }
};
} // namespace Engine
