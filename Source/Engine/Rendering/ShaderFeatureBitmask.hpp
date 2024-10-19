#pragma once
#include <ThirdParty/xxHash/xxhash.h>
#include <bitset>
#include <cinttypes>

struct ShaderFeatureBitmask
{
    ShaderFeatureBitmask(uint64_t i) : shaderFeature(i), vertFeature(0), fragFeature(0) {}
    ShaderFeatureBitmask() : shaderFeature(0), vertFeature(0), fragFeature(0) {}

    uint64_t shaderFeature;
    uint64_t vertFeature;
    uint64_t fragFeature;

    bool operator==(const ShaderFeatureBitmask& other) const
    {
        return shaderFeature == other.shaderFeature && vertFeature == other.vertFeature &&
               fragFeature == other.fragFeature;
    }

    ShaderFeatureBitmask operator|(const ShaderFeatureBitmask& other) const
    {
        ShaderFeatureBitmask rtn;

        rtn.shaderFeature = shaderFeature | other.shaderFeature;
        rtn.vertFeature = vertFeature | other.vertFeature;
        rtn.fragFeature = fragFeature | other.fragFeature;
        return rtn;
    }

    ShaderFeatureBitmask operator&(const ShaderFeatureBitmask& other) const
    {
        ShaderFeatureBitmask rtn;
        rtn.shaderFeature = shaderFeature & other.shaderFeature;
        rtn.vertFeature = vertFeature & other.vertFeature;
        rtn.fragFeature = fragFeature & other.fragFeature;
        return rtn;
    }
};

namespace std
{
template <>
struct hash<ShaderFeatureBitmask>
{
    size_t operator()(const ShaderFeatureBitmask& s) const
    {
        return XXH64(&s, sizeof(ShaderFeatureBitmask), 0);
    }
};

} // namespace std
