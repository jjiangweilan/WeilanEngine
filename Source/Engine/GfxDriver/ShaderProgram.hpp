#pragma once

#include "Core/Ptr.hpp"
#include "ShaderConfig.hpp"
#include "ShaderInfo.hpp"
#include <string>
#include <vector>
namespace Gfx
{
struct ShaderResourceLayout
{
    std::string name;
    uint32_t size;
    uint32_t offset;
};

class ShaderProgram
{
public:
    ShaderProgram(bool isCompute) : isCompute(isCompute) {}
    virtual ~ShaderProgram() {};
    virtual const ShaderConfig& GetDefaultShaderConfig() = 0;
    virtual const std::string& GetName() = 0;
    virtual const ShaderInfo::ShaderInfo& GetShaderInfo() = 0;
    bool IsCompute() { return isCompute; }
    const UUID& GetUUID() const { return uuid; }

protected:
    bool isCompute;
    UUID uuid;
};
} // namespace Gfx
