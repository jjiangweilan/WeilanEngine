#pragma once

namespace Gfx
{
    class ImageIdentifier;
}

namespace Rendering
{
struct RenderingData;
class RenderPipelinePass
{
public:
    virtual void OnInit(RenderingData* renderingData) {};
    virtual bool DebugBlit(Gfx::ImageIdentifier& dst) { return false; }
    virtual ~RenderPipelinePass() = default;
};
} // namespace Rendering
