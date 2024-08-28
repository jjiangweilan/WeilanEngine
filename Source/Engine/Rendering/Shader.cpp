#include "Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
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
    assert(shaderPassIndex >= 0 && shaderPassIndex < shaderPasses.size());

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
    assert(shaderPassIndex >= 0 && shaderPassIndex < shaderPasses.size());

    ShaderFeatureBitmask id = 0;
    for (auto& f : enabledFeature)
    {
        id = id | shaderPasses[shaderPassIndex]->featureToBitMask[f];
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
    assert(shaderPasses.size() != 0);
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

bool Shader::LoadFromFile(const char* path)
{
    if (name.empty())
    {
        std::filesystem::path apath(path);
        this->name = apath.string();
    }
    ShaderCompiler compiler;
    std::fstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string ssf = ss.str();

    // preprocess shader pass
    std::regex shaderPassReg("ShaderPass\\s+(\\w+)");

    std::vector<std::unique_ptr<ShaderPass>> newShaderPasses{};
    std::set<std::filesystem::path> includedFilesSet{};

    for (std::sregex_iterator it(ssf.begin(), ssf.end(), shaderPassReg), it_end; it != it_end; ++it)
    {
        std::string shaderPassName = it->str(1);
        int nameStart = it->position(1);
        int nameLength = it->length(1);

        int index = nameStart + nameLength;
        // find first '{'
        while (ssf[index] != '{' && index < ssf.size())
        {
            index++;
        }

        if (ssf[index] != '{')
            throw std::runtime_error("failed to found valid shader pass block");
        int quoteCount = 1;
        int shaderPassBlockStart = index + 1;
        index += 1;
        while (index < ssf.size() && quoteCount != 0)
        {
            if (ssf[index] == '{')
            {
                quoteCount += 1;
            }
            if (ssf[index] == '}')
            {
                quoteCount -= 1;
            }
            index++;
        }
        if (ssf[index - 1] != '}')
            throw std::runtime_error("failed to found valid shader pass block");
        int shaderPassBlockEnd = index - 1;

        // compile shader pass block
        auto shaderPass = std::make_unique<ShaderPass>();

        std::string shaderPassBlock = ssf.substr(shaderPassBlockStart, shaderPassBlockEnd - shaderPassBlockStart);
        try
        {
            compiler.Compile(path, shaderPassBlock);
        }
        catch (std::exception e)
        {
            SPDLOG_ERROR("{}, {}", e.what(), path);
            return false;
        }

        auto& includedFiles = compiler.GetIncludedFiles();
        includedFilesSet.insert(includedFiles.begin(), includedFiles.end());

        for (auto& iter : compiler.GetCompiledSpvs())
        {
            try
            {
                shaderPass->shaderPrograms[iter.first] =
                    GetGfxDriver()->CreateShaderProgram(compiler.GetName(), compiler.GetConfig(), iter.second);
            }
            catch (std::exception e)
            {
                SPDLOG_ERROR(e.what());
            }
        }

        shaderPass->name = shaderPassName;
        shaderPass->featureToBitMask = compiler.GetFeatureToBitMask();
        newShaderPasses.push_back(std::move(shaderPass));
    }

    // no shader pass use the whole as single pass
    if (newShaderPasses.empty())
    {
        auto shaderPass = std::make_unique<ShaderPass>();

        compiler.Compile(path, ssf);
        auto& includedFiles = compiler.GetIncludedFiles();
        includedFilesSet.insert(includedFiles.begin(), includedFiles.end());
        for (auto& iter : compiler.GetCompiledSpvs())
        {
            shaderPass->shaderPrograms[iter.first] =
                GetGfxDriver()->CreateShaderProgram(compiler.GetName(), compiler.GetConfig(), iter.second);
        }

        shaderPass->name = compiler.GetName();
        shaderPass->featureToBitMask = compiler.GetFeatureToBitMask();
        shaderPass->cachedShaderProgram = nullptr;
        newShaderPasses.push_back(std::move(shaderPass));
    }
    // this->name = compiler.GetName();

    shaderPasses = std::move(newShaderPasses);
    contentHash += 1;

    // update included files
    includedFiles.files = std::vector(includedFilesSet.begin(), includedFilesSet.end());
    includedFiles.lastWriteTime.clear();
    for (auto& f : includedFiles.files)
    {
        includedFiles.lastWriteTime.push_back(std::filesystem::last_write_time(f).time_since_epoch().count());
    }

    return true;
}

bool ComputeShader::LoadFromFile(const char* path)
{
    auto shaderPass = std::make_unique<ShaderPass>();

    shaderPass->cachedShaderProgram = nullptr;

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
        compiler.CompileComputeShader(path, ss.str());
        auto& includedFiles = compiler.GetIncludedFiles();
        for (auto& iter : compiler.GetCompiledSpvs())
        {
            shaderPass->shaderPrograms[iter.first] =
                GetGfxDriver()->CreateShaderProgram(compiler.GetName(), compiler.GetConfig(), iter.second);
        }

        this->name = compiler.GetName();

        shaderPass->featureToBitMask = compiler.GetFeatureToBitMask();
        shaderPasses.clear();
        shaderPasses.push_back(std::move(shaderPass));

        // update included files
        this->includedFiles.files = std::vector(includedFiles.begin(), includedFiles.end());
        this->includedFiles.lastWriteTime.clear();
        // update included files
        for (auto& f : this->includedFiles.files)
        {
            this->includedFiles.lastWriteTime.push_back(std::filesystem::last_write_time(f).time_since_epoch().count());
        }
        contentHash += 1;
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR("{} {}", e.what(), path);
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
