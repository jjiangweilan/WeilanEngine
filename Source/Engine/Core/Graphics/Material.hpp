#pragma once

#include "Core/AssetObject.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Core/Texture.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Shader.hpp"
#include <string>
#include <glm/glm.hpp>
#include <unordered_map>
namespace Engine
{
    namespace Gfx
    {
        class ShaderResource;
    }

    class AssetDatabase;
    class AssetFileData;
    class Material : public AssetObject
    {
        DECLARE_ENGINE_OBJECT();

        public:
            Material(std::string_view shader);
            Material();
            ~Material() override;

            void SetShader(std::string_view shader);
            RefPtr<Shader> GetShader() { return shader; }
            RefPtr<Gfx::ShaderResource> GetShaderResource() { return shaderResource; }

            void SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value);
            void SetFloat(const std::string& param, const std::string& member, float value);
            void SetVector(const std::string& param, const std::string& member, const glm::vec4& value);
            void SetTexture(const std::string& param, RefPtr<Gfx::Image> image);
            void SetTexture(const std::string& param, RefPtr<Texture> texture);
            void SetTexture(const std::string& param, std::nullptr_t);

            glm::mat4 GetMatrix(const std::string& param, const std::string& membr);
            RefPtr<Texture> GetTexture(const std::string& param);
            float            GetFloat(const std::string& param, const std::string& membr);
            glm::vec4        GetVector(const std::string& param, const std::string& membr);

            // const UUID& Serialize(RefPtr<AssetFileData> assetFileData) override;
            // void        Deserialize(RefPtr<AssetFileData> assetFileData, RefPtr<AssetDatabase> assetDatabase) override;

            Gfx::ShaderConfig& GetShaderConfig() { return shaderConfig; }
            void DeserializeInternal(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver) override;

        private:
                RefPtr<Shader> shader = nullptr;
                UniPtr<Gfx::ShaderResource> shaderResource;
                Gfx::ShaderConfig shaderConfig;

                std::string shaderName;
                std::unordered_map<std::string, float> floatValues;
                std::unordered_map<std::string, glm::vec4> vectorValues;
                std::unordered_map<std::string, glm::mat4> matrixValues;
                std::unordered_map<std::string, RefPtr<Texture>> textureValues;

                AssetDatabase::OnAssetReloadIterHandle assetReloadIterHandle;

                void UpdateResources();
                void SetShaderNoProtection(std::string_view shader);
                void OnReferenceResolve(void* ptr, AssetObject* resolved) override;
                static int initImporter_;
    };
}
