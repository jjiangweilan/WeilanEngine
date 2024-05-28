#pragma once
#include "GfxDriver/CommandBuffer.hpp"

class RenderPipeline
{
public:
    RenderPipeline();

    void Setup();
    void Execute(Gfx::CommandBuffer& cmd);
};
