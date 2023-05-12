#pragma once
#include "Libs/Ptr.hpp"
#include <vector>

namespace Engine::RenderGraphImpl
{

struct RenderAttachment
{};

struct Subpass
{};

class RenderPass
{
public:
    void SetAttachment(int index, RefPtr<RenderAttachment> resource);
    void AddSubpass(Subpass pass);
    virtual void Execute() = 0;
    void ClearAttachment();

protected:
    std::vector<RenderAttachment> attachments;
};
} // namespace Engine::RenderGraphImpl
