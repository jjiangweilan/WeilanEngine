#pragma once

#include "GfxEnums.hpp"
#include "StorageBuffer.hpp"
#include "Code/Ptr.hpp"
#include <unordered_map>
#include <string>

namespace Engine::Gfx
{
    class ShaderProgram;
    class Image;

    class ShaderResource
    {
        public:
            virtual ~ShaderResource() {};
            void SetUniform(std::string_view param, void* value);
            virtual void SetUniform(std::string_view obj, std::string_view member, void* value) = 0;
            virtual void SetTexture(const std::string& name, RefPtr<Image> texture) = 0;
            virtual void SetStorage(std::string_view name, RefPtr<StorageBuffer> storageBuffer) = 0;
            virtual RefPtr<ShaderProgram> GetShader() = 0;
    };
}
