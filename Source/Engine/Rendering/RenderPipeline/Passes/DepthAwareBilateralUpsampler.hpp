#pragma once
#include "Rendering/Material.hpp"
#include "Rendering/Shader2.hpp"

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
        const Gfx::RG::ImageIdentifier& lowResColor,
        const Gfx::RG::ImageIdentifier& lowResDepth,
        const Gfx::RG::ImageIdentifier& highResDepth,
        const Gfx::RG::ImageIdentifier& destination,
        const GPUInput& gpuInput
    );

    void Execute(Gfx::CommandBuffer& cmd);

private:
    ObjPtr<Shader2> shader;

    Material resource;

    Gfx::RG::ImageIdentifier lowResColor;
    Gfx::RG::ImageIdentifier lowResDepth;
    Gfx::RG::ImageIdentifier highResDepth;
    Gfx::RG::ImageIdentifier destination;
    GPUInput gpuInput{};
};
} // namespace Rendering::Passes
