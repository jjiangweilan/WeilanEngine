#include "RenderCommandBuffer.hpp"

void RenderComandBuffer::AllocateTempImage(const RenderImageDescriptor& desc, ImageIdentifier& id)
{
    resourceAllocator->AllocateTempImage(desc, id);
}

void RenderComandBuffer::BeginLabel(std::string_view label)
{
    char* extraData = nullptr;
    BeginLabelCmd* ptr = cm.Push<BeginLabelCmd>(
        &BeginLabelCmd::Execute,
        &extraData,
        label.length() + 1
    );

    memcpy(extraData, label.data(), label.length() + 1);
    ptr->str = extraData;
}
