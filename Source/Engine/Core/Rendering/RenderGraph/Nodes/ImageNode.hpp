#pragma once
#include "../Node.hpp"

namespace Engine::RGraph
{
class ImageNode : public Node
{
public:
    enum class Type
    {
        Color,
        Depth
    };

    ImageNode();
    bool Preprocess(ResourceStateTrack& stateTrack) override;
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(ResourceStateTrack& stateTrack) override;

    uint32_t width;
    uint32_t height;
    Gfx::ImageFormat format;
    Gfx::MultiSampling multiSampling;
    uint32_t mipLevels;
    Type type;

    Port* GetPortOutput();

private:
    Port* outputPort;

    /**
     * the image this node is holding
     */
    UniPtr<Gfx::Image> image = nullptr;
};
} // namespace Engine::RGraph
