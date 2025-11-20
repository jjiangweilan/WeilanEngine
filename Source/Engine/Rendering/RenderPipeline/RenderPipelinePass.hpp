#pragma once

namespace Rendering
{
struct RenderingData;
class RenderPipelinePass
{
public:
    virtual void OnInit(RenderingData* renderingData) {};
    virtual ~RenderPipelinePass() = default;
};
} // namespace Rendering
