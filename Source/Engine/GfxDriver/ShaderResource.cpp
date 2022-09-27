#include "ShaderResource.hpp"
namespace Engine::Gfx
{
    void ShaderResource::SetUniform(std::string_view param, void* value)
    {
        size_t dotPos = param.find('.');
        std::string_view objName = param.substr(0, dotPos);
        std::string_view memName = param.substr(dotPos + 1);
        SetUniform(objName, memName, value);
    }
}