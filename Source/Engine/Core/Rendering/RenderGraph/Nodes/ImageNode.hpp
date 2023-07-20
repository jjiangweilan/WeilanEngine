#pragma once
#include "../Node.hpp"

namespace Engine::RGraph
{
class ImageNode : public Node
{
public:
    ImageNode();
    bool Preprocess(ResourceStateTrack& stateTrack) override;
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(Gfx::CommandBuffer* cmdBuf, ResourceStateTrack& stateTrack) override;
    void SetExternalImage(Gfx::Image* externalImage) { this->externalImage = externalImage; };

    uint32_t width;
    uint32_t height;
    Gfx::ImageFormat format;
    Gfx::MultiSampling multiSampling = Gfx::MultiSampling::Sample_Count_1;
    uint32_t mipLevels = 1;
    ResourceState initialState;

    Port* GetPortOutput() { return outputPort; };

private:
    Port* outputPort;

    // the image this node is holding
    UniPtr<Gfx::Image> image = nullptr;

    Gfx::Image* externalImage = nullptr;
    // initial state of the external image when the graph executes
    ResourceRef imageRes;
};
} // namespace Engine::RGraph
