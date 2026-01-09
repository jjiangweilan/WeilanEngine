#pragma once
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/Rendering/GPUBuffer.hpp"
#include <vector>
#include <string>

namespace Rendering::Passes
{
struct BloomInput
{
    glm::vec4 texelSize;
    glm::vec4 params;
    float mode;
};

class BloomPass : public RenderPipelinePass
{
public:
    BloomPass();
    ~BloomPass();

    void Execute(
        Gfx::CommandBuffer& cmd,
        Gfx::ImageIdentifier srcColor,
        Gfx::RenderImageDescriptor srcDesc,
        const RenderPipelineSetting::PostProcess::Bloom& settings
    );

    Gfx::ImageIdentifier GetOutput() { return finalOutput; }

private:
    ObjPtr<Shader> shader;
    GPUBuffer<BloomInput> bloomInputBuffer;
    
    struct MipLevel
    {
        Gfx::ImageIdentifier texture;
        Gfx::RenderImageDescriptor desc;
        int width;
        int height;
    };
    std::vector<MipLevel> mipChain;
    Gfx::ImageIdentifier finalOutput;
    
    const int kMaxMips = 6;
};
} // namespace Rendering::Passes
