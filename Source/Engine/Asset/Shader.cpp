#include "Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include "ThirdParty/xxHash/xxhash.h"
#include <spdlog/spdlog.h>

DEFINE_ASSET(Shader, "41EF74E2-6DAF-4755-A385-ABFCC4E83147", "shad");
DEFINE_ASSET(ComputeShader, "91093726-6D8F-440C-B617-6AA3FDA08DEA", "comp");

ShaderBase::ShaderBase(const std::string& name, std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid)
{
    shaderPrograms[0] = std::move(shaderProgram);
    SetUUID(uuid);
    this->name = name;
}

void ShaderBase::Reload(Asset&& other)
{
    ShaderBase* casted = static_cast<ShaderBase*>(&other);
    shaderName = (std::move(casted->shaderName));
    shaderPrograms = (std::move(casted->shaderPrograms));
    cachedShaderProgram = nullptr;
    contentHash = 0;
    featureToBitMask = std::move(casted->featureToBitMask);
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

Gfx::ShaderProgram* ShaderBase::GetShaderProgram(size_t enabledFeatureHash)
{
    for (auto& s : shaderPrograms)
    {
        if ((enabledFeatureHash & s.first) == enabledFeatureHash)
        {
            return s.second.get();
        }
    }

    return nullptr;
}

uint64_t ShaderBase::GetFeaturesID(const std::vector<std::string>& enabledFeature)
{
    uint64_t id = 0;
    for (auto& f : enabledFeature)
    {
        id |= featureToBitMask[f];
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

Gfx::ShaderProgram* ShaderBase::GetShaderProgram(const std::vector<std::string>& enabledFeature)
{
    uint64_t id = GetFeaturesID(enabledFeature);

    return GetShaderProgram(id);
}

Gfx::ShaderProgram* ShaderBase::GetDefaultShaderProgram()
{
    uint64_t globalShaderFeaturesHash = ShaderBase::GetEnabledFeaturesHash();

    if (cachedShaderProgram == nullptr || this->globalShaderFeaturesHash != globalShaderFeaturesHash)
    {
        this->globalShaderFeaturesHash = globalShaderFeaturesHash;
        auto& globalEnabledFeatures = ShaderBase::GetEnabledFeatures();
        cachedShaderProgram =
            GetShaderProgram(std::vector<std::string>(globalEnabledFeatures.begin(), globalEnabledFeatures.end()));
    }

    return cachedShaderProgram;
}

ShaderBase::GlobalShaderFeature& ShaderBase::GetGlobalShaderFeature()
{
    static GlobalShaderFeature f;
    return f;
}

bool Shader::LoadFromFile(const char* path)
{
    cachedShaderProgram = nullptr;

    if (name.empty())
    {
        std::filesystem::path apath(path);
        this->name = apath.string();
    }
    ShaderCompiler compiler;
    std::fstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    try
    {
        bool debugMode = false;
#if ENGINE_EDITOR
        debugMode = true;
#endif
        compiler.Compile(ss.str(), debugMode);
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
bool ComputeShader::LoadFromFile(const char* path)
{
    if (name.empty())
    {
        std::filesystem::path apath(path);
        this->name = apath.string();
    }
    ShaderCompiler compiler;
    std::filesystem::path p(path);
    auto ext = p.extension();
    std::fstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    try
    {
        bool debugMode = false;
#if ENGINE_EDITOR
        debugMode = true;
#endif

        compiler.CompileComputeShader(ss.str(), debugMode);
        for (auto& iter : compiler.GetCompiledSpvs())
        {
            shaderPrograms[iter.first] =
                GetGfxDriver()->CreateComputeShaderProgram(path, compiler.GetConfig(), iter.second.compSpv);
        }

        this->name = compiler.GetName();
        contentHash += 1;

        featureToBitMask = compiler.GetFeatureToBitMask();
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR("{}", e.what());
        return false;
    }

    return true;
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
