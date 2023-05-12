#pragma once

#include "Core/Resource.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Shader.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
namespace Engine
{
namespace Gfx
{
class ShaderResource;
}

class AssetDatabase;
class AssetFileData;
class Material : public Resource
{
public:
    Material(RefPtr<Shader> shader);
    Material();
    ~Material() override;

    void SetShader(RefPtr<Shader> shader);
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
    float GetFloat(const std::string& param, const std::string& membr);
    glm::vec4 GetVector(const std::string& param, const std::string& membr);

    // const UUID& Serialize(RefPtr<AssetFileData> assetFileData) override;
    // void        Deserialize(RefPtr<AssetFileData> assetFileData, RefPtr<AssetDatabase> assetDatabase) override;

    Gfx::ShaderConfig& GetShaderConfig() { return shaderConfig; }

private:
    RefPtr<Shader> shader = nullptr;
    UniPtr<Gfx::ShaderResource> shaderResource;
    Gfx::ShaderConfig shaderConfig;

    std::unordered_map<std::string, float> floatValues;
    std::unordered_map<std::string, glm::vec4> vectorValues;
    std::unordered_map<std::string, glm::mat4> matrixValues;
    std::unordered_map<std::string, RefPtr<Texture>> textureValues;

    void UpdateResources();
    void SetShaderNoProtection(RefPtr<Shader> shader);
    static int initImporter_;

    friend class SerializableField<Material>;
};

template <>
struct SerializableField<Material>
{
    static SerializableFileID GetSerializableFileID() { return GENERATE_SERIALIZABLE_FILE_ID("Material"); }

    static void Serilaize(Material* m, Serializer* s)
    {
        SerializableField<Resource>::Serialize(m, s);
        s->Serialize(m->shader);
        s->Serialize(m->floatValues);
        s->Serialize(m->vectorValues);
        s->Serialize(m->matrixValues);
        s->Serialize(m->textureValues);
    }

    static void Deserialize(Material* m, Serializer* s)
    {
        SerializableField<Resource>::Deserialize(m, s);
        s->Deserialize(m->shader, [m](void* res) { m->SetShader(m->shader); });
        s->Deserialize(m->floatValues);
        s->Deserialize(m->vectorValues);
        s->Deserialize(m->matrixValues);
        s->Deserialize(m->textureValues,
                       [m](void* res)
                       {
                           if (res)
                           {
                               Texture* tex = (Texture*)res;
                               for (auto& kv : m->textureValues)
                               {
                                   if (kv.second.Get() == tex)
                                   {
                                       m->SetTexture(kv.first, tex);
                                       break;
                                   }
                               }
                           }
                       });
    }
};
} // namespace Engine
