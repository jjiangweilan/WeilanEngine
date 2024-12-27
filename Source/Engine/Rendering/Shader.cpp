#include "Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Assert.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <spdlog/spdlog.h>

DEFINE_ASSET(Shader, "41EF74E2-6DAF-4755-A385-ABFCC4E83147", "shad");
DEFINE_ASSET(ComputeShader, "91093726-6D8F-440C-B617-6AA3FDA08DEA", "comp");

ShaderBase::ShaderBase(const std::string& name, std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid)
{
    auto shaderPass = std::make_unique<ShaderPass>();
    shaderPass->shaderPrograms[0] = std::move(shaderProgram);
    shaderPass->name = "Default";
    shaderPasses.push_back(std::move(shaderPass));
    SetUUID(uuid);
    this->name = name;
}

void ShaderBase::Reload(Asset&& other)
{
    ShaderBase* casted = static_cast<ShaderBase*>(&other);
    shaderName = (std::move(casted->shaderName));
    shaderPasses = std::move(casted->shaderPasses);
    includedFiles = std::move(casted->includedFiles);
    for (auto& s : shaderPasses)
    {
        s->cachedShaderProgram = nullptr;
    }
    contentHash = 0;
    Asset::Reload(std::move(other));
}

void ShaderBase::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("shaderName", shaderName);
}

void ShaderBase::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("shaderName", shaderName);
}

uint32_t ShaderBase::GetContentHash()
{
    return contentHash;
}

const std::set<std::string>& ShaderBase::GlobalShaderFeature::GetEnabledFeatures()
{
    return enabledFeatures;
}

uint64_t ShaderBase::GlobalShaderFeature::GetEnabledFeaturesHash()
{
    return setHash;
}

void ShaderBase::GlobalShaderFeature::EnableFeature(const char* name)
{
    if (!enabledFeatures.contains(name))
    {
        enabledFeatures.emplace(name);
        Rehash();
    }
}

int ShaderBase::FindShaderPass(std::string_view name)
{
    for (int i = 0; i < shaderPasses.size(); ++i)
    {
        if (shaderPasses[i]->name == name)
            return i;
    }

    return -1;
}

Gfx::ShaderProgram* ShaderBase::GetShaderProgram(const ShaderFeatureBitmask& enabledFeatureHash)
{
    return GetShaderProgram(0, enabledFeatureHash);
}

Gfx::ShaderProgram* ShaderBase::GetShaderProgram(
    int shaderPassIndex, const ShaderFeatureBitmask& enabledShaderFeatureBismask
)
{
    ASSERT(shaderPassIndex >= 0 && shaderPassIndex < shaderPasses.size());

    for (auto& s : shaderPasses[shaderPassIndex]->shaderPrograms)
    {
        if ((enabledShaderFeatureBismask == s.first))
        {
            return s.second.get();
        }
    }

    return nullptr;
}

ShaderFeatureBitmask ShaderBase::GetShaderFeatureBitmask(
    int shaderPassIndex, const std::vector<std::string>& enabledFeature
)
{
    ASSERT(shaderPassIndex >= 0 && shaderPassIndex < shaderPasses.size());

    ShaderFeatureBitmask id = 0;
    for (auto& f : enabledFeature)
    {
        id = id | shaderPasses[shaderPassIndex]->featureToBitmask[f];
    }

    return id;
}

void ShaderBase::GlobalShaderFeature::DisableFeature(const char* name)
{
    if (enabledFeatures.contains(name))
    {
        enabledFeatures.erase(name);
        Rehash();
    }
}

void ShaderBase::GlobalShaderFeature::Rehash()
{
    std::string concat = "";
    concat.reserve(512);

    for (auto& f : enabledFeatures)
    {
        concat += f;
    }

    setHash = XXH64(concat.data(), concat.size(), 0);
}

Gfx::ShaderProgram* ShaderBase::GetDefaultShaderProgram()
{
    ASSERT(shaderPasses.size() != 0);
    auto& shaderPass = shaderPasses[0];

    uint64_t globalShaderFeaturesHash = ShaderBase::GetEnabledFeaturesHash();

    if (shaderPass->cachedShaderProgram == nullptr || shaderPass->globalShaderFeaturesHash != globalShaderFeaturesHash)
    {
        shaderPass->globalShaderFeaturesHash = globalShaderFeaturesHash;
        auto& globalEnabledFeatures = ShaderBase::GetEnabledFeatures();
        shaderPass->cachedShaderProgram =
            GetShaderProgram(std::vector<std::string>(globalEnabledFeatures.begin(), globalEnabledFeatures.end()));
    }

    return shaderPass->cachedShaderProgram;
}

ShaderBase::GlobalShaderFeature& ShaderBase::GetGlobalShaderFeature()
{
    static GlobalShaderFeature f;
    return f;
}

void Shader::SetDefault(Shader* defaultShader)
{
    GetDefaultPrivate() = defaultShader;
}
Shader* Shader::GetDefault()
{
    return GetDefaultPrivate();
}

Shader*& Shader::GetDefaultPrivate()
{
    static Shader* shader;
    return shader;
}

bool ShaderBase::NeedReimport()
{
    for (int i = 0; i < includedFiles.lastWriteTime.size() && i < includedFiles.files.size(); i++)
    {
        if (includedFiles.lastWriteTime[i] <
            std::filesystem::last_write_time(includedFiles.files[i]).time_since_epoch().count())
            return true;
    }
    return false;
}

bool Shader::LoadFromFile(const char* path)
{
    return false;
}
