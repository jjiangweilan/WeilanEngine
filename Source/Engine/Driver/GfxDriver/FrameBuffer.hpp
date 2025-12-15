#pragma once
#include "Core/Ptr.hpp"
#include "Library/DynamicArray.hpp"
namespace Gfx
{
class RenderPass_Deprecated;
class Image;
class FrameBuffer
{
public:
    virtual void SetAttachments(const std::vector<RefPtr<Image>>& attachments) = 0;
    virtual ~FrameBuffer() {}
};
} // namespace Gfx
