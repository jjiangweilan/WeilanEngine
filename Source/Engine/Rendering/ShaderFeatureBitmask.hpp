#include <ThirdParty/xxHash/xxhash.h>
#include <bitset>
#include <cinttypes>

struct ShaderFeatureBitmask
{
    ShaderFeatureBitmask(uint64_t shaderFeature) : shaderFeature(shaderFeature), vertFeature(0), fragFeature(0) {}
    uint64_t shaderFeature;
    uint64_t vertFeature;
    uint64_t fragFeature;
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
