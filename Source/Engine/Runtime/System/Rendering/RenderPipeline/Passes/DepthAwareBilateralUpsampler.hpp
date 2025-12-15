#pragma once
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"

namespace Rendering::Passes
{
class DepthAwareBilateralUpsampler
{
public:
    struct GPUInput
    {
        bool operator==(const GPUInput& other) const = default;

        glm::int2 highResTexSize;
        float kernelSize;
        float integerCoordSigma;
        float depthDiffSigma;
    };

    DepthAwareBilateralUpsampler();

    void Setup(
        const Gfx::ImageIdentifier& lowResColor,
        const Gfx::ImageIdentifier& lowResDepth,
        const Gfx::ImageIdentifier& highResDepth,
        const Gfx::ImageIdentifier& destination,
        const GPUInput& gpuInput
    );

    void Execute(Gfx::CommandBuffer& cmd);

private:
    ObjPtr<Shader> shader;

    Material resource;

    Gfx::ImageIdentifier lowResColor;
    Gfx::ImageIdentifier lowResDepth;
    Gfx::ImageIdentifier highResDepth;
    Gfx::ImageIdentifier destination;
    GPUInput gpuInput{};
};
} // namespace Rendering::Passes
