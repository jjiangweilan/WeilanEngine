#pragma once

#include <string>
#include <vector>
#include "ShaderConfig.hpp"
#include "Code/Ptr.hpp"
namespace Engine::Gfx
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
            virtual ~ShaderProgram() {};
            virtual const ShaderConfig& GetDefaultShaderConfig() = 0;
            virtual const std::string& GetName() = 0;

        protected:
    };
}
