#pragma once
#include "Engine/Core/Object.hpp"
#include "GfxEnums.hpp"

namespace Gfx
{

class Sampler : public Object
{
public:
    struct CreateInfo
    {
        SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
        SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
        SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
        FilterMode minFilter = FilterMode::Linear;
        FilterMode magFilter = FilterMode::Linear;
        SamplerMipmapMode mipmapMode = SamplerMipmapMode::Linear;
        bool anisotropic = false;
        bool enableCompare = false;
        const char* debugName = nullptr;
    };

    Sampler()
        : Object() {}
    virtual ~Sampler() {}
};

} // namespace Gfx
