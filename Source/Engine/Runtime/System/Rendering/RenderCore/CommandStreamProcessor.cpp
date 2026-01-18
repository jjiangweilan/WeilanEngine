#include "CommandStreamProcessor.hpp"

void CommandStreamProcessor::BeginLabel(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    BeginLabelCmd* cmd = (BeginLabelCmd*)cmdData;
    self->gfxCmdBuf->BeginLabel(cmd->str, cmd->color);
}

void CommandStreamProcessor::EndLabel(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    (void)cmdData;
    self->gfxCmdBuf->EndLabel();
}

void CommandStreamProcessor::SetRenderPass(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetRenderPassCmd* cmd = (SetRenderPassCmd*)cmdData;
    (void)self;
    (void)cmd;
}

void CommandStreamProcessor::SetClearValues(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetClearValuesCmd* cmd = (SetClearValuesCmd*)cmdData;
    (void)self;
    (void)cmd;
}

void CommandStreamProcessor::BindResource(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    BindResourceCmd* cmd = (BindResourceCmd*)cmdData;
    self->gfxCmdBuf->BindResource(cmd->set, cmd->resource);
}

void CommandStreamProcessor::BindDynamicBindings(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    BindDynamicBindingsCmd* cmd = (BindDynamicBindingsCmd*)cmdData;

    self->gfxCmdBuf->BindResource(cmd->set, std::vector<Gfx::DynamicBinding>(cmd->bindings, cmd->bindings + cmd->count));
}

void CommandStreamProcessor::SetPushConstant(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetPushConstantCmd* cmd = (SetPushConstantCmd*)cmdData;
    (void)self;
    (void)cmd;
}

void CommandStreamProcessor::DrawMesh(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    DrawMeshCmd* cmd = (DrawMeshCmd*)cmdData;
    (void)self;
    (void)cmd;
}

void CommandStreamProcessor::Draw(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    DrawCmd* cmd = (DrawCmd*)cmdData;
    self->gfxCmdBuf->Draw(cmd->vertexCount, cmd->instanceCount, cmd->firstVertex, cmd->firstInstance);
}

void CommandStreamProcessor::DrawIndirect(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    DrawIndirectCmd* cmd = (DrawIndirectCmd*)cmdData;
    self->gfxCmdBuf->DrawIndirect(cmd->buffer, cmd->offset, cmd->drawCount, cmd->stride);
}

void CommandStreamProcessor::DrawIndexedIndirect(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    DrawIndexedIndirectCmd* cmd = (DrawIndexedIndirectCmd*)cmdData;
    self->gfxCmdBuf->DrawIndexedIndirect(cmd->buffer, cmd->offset, cmd->drawCount, cmd->stride);
}

void CommandStreamProcessor::Blit(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    BlitCmd* cmd = (BlitCmd*)cmdData;
    self->gfxCmdBuf->Blit(cmd->src, cmd->dst, cmd->op);
}

void CommandStreamProcessor::SetScissor(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetScissorCmd* cmd = (SetScissorCmd*)cmdData;
    self->gfxCmdBuf->SetScissor(cmd->firstScissor, cmd->scissorCount, cmd->rects);
}

void CommandStreamProcessor::SetViewport(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetViewportCmd* cmd = (SetViewportCmd*)cmdData;
    self->gfxCmdBuf->SetViewport(cmd->viewport);
}

void CommandStreamProcessor::SetLineWidth(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetLineWidthCmd* cmd = (SetLineWidthCmd*)cmdData;
    self->gfxCmdBuf->SetLineWidth(cmd->lineWidth);
}

void CommandStreamProcessor::SetDepthBias(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetDepthBiasCmd* cmd = (SetDepthBiasCmd*)cmdData;
    self->gfxCmdBuf->SetDepthBias(cmd->constantFactor, cmd->clamp, cmd->slopeFactor);
}

void CommandStreamProcessor::SetDepthBiasEnable(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    SetDepthBiasEnableCmd* cmd = (SetDepthBiasEnableCmd*)cmdData;
    self->gfxCmdBuf->SetDepthBiasEnable(cmd->enable);
}

void CommandStreamProcessor::Dispatch(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    DispatchCmd* cmd = (DispatchCmd*)cmdData;
    self->gfxCmdBuf->Dispatch(cmd->groupCountX, cmd->groupCountY, cmd->groupCountZ);
}

void CommandStreamProcessor::DispatchIndirect(CommandStreamContext* selfPtr, void* cmdData)
{
    CommandStreamProcessor* self = static_cast<CommandStreamProcessor*>(selfPtr);
    DispatchIndirectCmd* cmd = (DispatchIndirectCmd*)cmdData;
    self->gfxCmdBuf->DispatchIndirect(cmd->buffer, cmd->bufferOffset);
}
