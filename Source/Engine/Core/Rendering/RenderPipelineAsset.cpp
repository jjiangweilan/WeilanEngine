#include "RenderPipeline.hpp"

namespace Engine::Rendering
{
    RenderPipelineAsset::RenderPipelineAsset()
    {
        SERIALIZE_MEMBER(virtualTexture);
    }
}
