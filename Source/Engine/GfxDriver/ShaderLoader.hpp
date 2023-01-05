#pragma once
#include <string>
#include "Libs/Ptr.hpp"
namespace Engine::Gfx
{
    class ShaderProgram;
    class ShaderModule;
    class ShaderLoader
    {
        public:
            virtual ~ShaderLoader() {};
            virtual RefPtr<ShaderProgram> GetShader(const std::string& name) = 0;
            virtual RefPtr<ShaderModule> GetShaderModule(const std::string& path) = 0;
            static RefPtr<ShaderLoader> Instance() { return singleton; }
            static  void InitShaderLoader(RefPtr<ShaderLoader> loader) { singleton = loader; }
        private:
            static RefPtr<ShaderLoader> singleton;
    };
}