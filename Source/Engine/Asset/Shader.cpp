#include "Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <spdlog/spdlog.h>

namespace Engine
{
DEFINE_ASSET(Shader, "41EF74E2-6DAF-4755-A385-ABFCC4E83147", "shad");

Shader::Shader(const std::string& name, std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid)
{
    shaderPrograms[0] = std::move(shaderProgram);
    SetUUID(uuid);
    this->name = name;
}

Shader::Shader(const char* path)
{
    LoadFromFile(path);
}

void Shader::Reload(Asset&& other)
{
    Shader* casted = static_cast<Shader*>(&other);
    Asset::Reload(std::move(other));
    shaderName = (std::move(casted->shaderName));
    shaderPrograms = (std::move(casted->shaderPrograms));
}

void Shader::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("shaderName", shaderName);
}

void Shader::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("shaderName", shaderName);
}

bool Shader::LoadFromFile(const char* path)
{
    ShaderCompiler compiler;
    std::fstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    try
    {
        compiler.Compile(ss.str(), true);
        for (auto& iter : compiler.GetCompiledSpvs())
        {
            shaderPrograms[iter.first] =
                GetGfxDriver()
                    ->CreateShaderProgram(path, compiler.GetConfig(), iter.second.vertSpv, iter.second.fragSpv);
        }
        this->name = compiler.GetName();
        contentHash += 1;

        featureToBitMask = compiler.GetFeatureToBitMask();
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR("{}", e.what());
    }

    return true;
}

uint32_t Shader::GetContentHash()
{
    return contentHash;
}

const std::set<std::string>& Shader::GlobalShaderFeature::GetEnabledFeatures()
{
    return enabledFeatures;
}

uint64_t Shader::GlobalShaderFeature::GetEnabledFeaturesHash()
{
    return setHash;
}

void Shader::GlobalShaderFeature::EnableFeature(const char* name)
{
    if (!enabledFeatures.contains(name))
    {
        enabledFeatures.emplace(name);
        Rehash();
    }
}

void Shader::GlobalShaderFeature::DisableFeature(const char* name)
{
    if (enabledFeatures.contains(name))
    {
        enabledFeatures.erase(name);
        Rehash();
    }
}

void Shader::GlobalShaderFeature::Rehash()
{
    std::string concat = "";
    concat.reserve(512);

    for (auto& f : enabledFeatures)
    {
        concat += f;
    }

    setHash = XXH64(concat.data(), concat.size(), 0);
}

Gfx::ShaderProgram* Shader::GetShaderProgram(const std::vector<std::string>& enabledFeature)
{
    uint64_t id = 0;
    for (auto& f : enabledFeature)
    {
        id |= featureToBitMask[f];
    }

    for (auto& s : shaderPrograms)
    {
        if ((id & s.first) == id)
        {
            return s.second.get();
        }
    }

    return nullptr;
}

Shader::GlobalShaderFeature& Shader::GetGlobalShaderFeature()
{
    static GlobalShaderFeature f;
    return f;
}
} // namespace Engine
