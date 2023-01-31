#pragma once
#include "Libs/Ptr.hpp"
#include <vector>
namespace Engine::Gfx
{
class RenderPass;
class Image;
class FrameBuffer
{
public:
    virtual void SetAttachments(const std::vector<RefPtr<Image>>& attachments) = 0;
    virtual ~FrameBuffer() {}
};
} // namespace Engine::Gfx
