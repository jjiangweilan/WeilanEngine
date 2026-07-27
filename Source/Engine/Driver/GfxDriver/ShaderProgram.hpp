#pragma once

#include "CompiledSpv.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/DynamicArray.hpp"
#include "PipelineConfig.hpp"
#include <string>
namespace Gfx
{
struct ShaderResourceLayout
{
    std::string name;
    uint32_t size;
    uint32_t offset;
};

class ShaderProgram : public Object
{
public:
    ShaderProgram(bool isCompute) : isCompute(isCompute), shaderID(globalShaderID++) {}
    virtual ~ShaderProgram() {};
    virtual const PipelineConfig& GetDefaultPipelineConfig() = 0;
    virtual const std::string& GetName() const = 0;
    virtual const ShaderPipelineInfo& GetShaderInfo() = 0;
    virtual int GetBindingNum(Gfx::DescriptorSetSemantics descriptorSet, std::string_view name) = 0;
    bool IsCompute() { return isCompute; }
    const UUID& GetUUID() const { return uuid; }
    uint32_t GetShaderID() const { return shaderID; }

protected:
    bool isCompute;
    UUID uuid;
    uint32_t shaderID;
    static std::atomic_uint32_t globalShaderID;
};
} // namespace Gfx
