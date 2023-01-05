#pragma once
#include "Libs/Ptr.hpp"
#include "Core/AssetObject.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include <string>
namespace Engine
{
namespace Gfx
{
    class ShaderProgram;
    class ShaderLoader;
}
    class Shader : public AssetObject
    {
        public:
            Shader(const std::string& name, UniPtr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid = UUID::empty);
            void Reload(AssetObject&& loaded) override;
            ~Shader() override {}

            inline RefPtr<Gfx::ShaderProgram> GetShaderProgram() { return shaderProgram; }
            inline const Gfx::ShaderConfig& GetDefaultShaderConfig() { return shaderProgram->GetDefaultShaderConfig(); }

        private:
            std::string shaderName;

            bool Serialize(AssetSerializer&) override{return false;} // disable model saving

            UniPtr<Gfx::ShaderProgram> shaderProgram;
    };
}
