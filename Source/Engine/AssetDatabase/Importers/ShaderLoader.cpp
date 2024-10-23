#include "ShaderLoader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include <fstream>
#include <spirv_cross/spirv_reflect.hpp>

DEFINE_ASSET_LOADER(ShaderLoader, "shad,comp")

const std::vector<std::type_index>& ShaderLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Shader), typeid(ComputeShader)};
    return types;
}

bool ShaderLoader::ImportNeeded()
{
    if (!meta.contains("shaderFileLastWriteTime"))
        return true;

    if (meta["shaderFileLastWriteTime"] <
        std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count())
        return true;

    if (!meta.contains("includedFiles"))
        return true;

    nlohmann::json includedFiles = meta.value("includedFiles", nlohmann::json::array_t());

    for (int i = 0; i < includedFiles.size(); i++)
    {
        if (!std::filesystem::exists(includedFiles[i]["path"]) || includedFiles[i]["lastWriteTime"] <
            std::filesystem::last_write_time(includedFiles[i]["path"]).time_since_epoch().count())
            return true;
    }

    return false;
}
void ShaderLoader::Import()
{
    struct PassCompiledData
    {
        struct Feature
        {
            ShaderFeatureBitmask mask;
            Gfx::CompiledSpv compiledSpv;
        };

        std::string shaderPassName;
        std::unordered_map<std::string, ShaderFeatureBitmask> featureToBitmask;
        nlohmann::json shaderConfig;
        std::vector<Feature> features;
    };

    ShaderCompiler compiler;
    std::vector<PassCompiledData> compiledData;
    std::vector<uint8_t> binaryData = {};

    std::fstream f(absoluteAssetPath);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string ssf = ss.str();

    // preprocess shader pass
    std::regex shaderPassReg("ShaderPass\\s+(\\w+)");

    std::vector<std::unique_ptr<ShaderPass>> newShaderPasses{};
    std::set<std::filesystem::path> includedFilesSet{};
    bool isCompute = absoluteAssetPath.extension() == ".comp";
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
            if (isCompute)
                compiler.CompileComputeShader(absoluteAssetPath.string().c_str(), shaderPassBlock);
            else
                compiler.Compile(absoluteAssetPath.string().c_str(), shaderPassBlock);
        }
        catch (std::exception e)
        {
            SPDLOG_ERROR("{}, {}", e.what(), absoluteAssetPath.string());
            return;
        }

        auto& includedFiles = compiler.GetIncludedFiles();
        includedFilesSet.insert(includedFiles.begin(), includedFiles.end());

        PassCompiledData passCompiledData;
        for (auto& iter : compiler.GetCompiledSpvs())
        {
            PassCompiledData::Feature group;
            group.mask = iter.first;
            group.compiledSpv = iter.second;
            passCompiledData.features.push_back(group);
        }

        passCompiledData.featureToBitmask = compiler.GetFeatureToBitMask();
        passCompiledData.shaderPassName = shaderPassName;
        passCompiledData.shaderConfig = compiler.GetConfig()->ToJson();
        compiledData.push_back(passCompiledData);
    }

    // no shader pass use the whole as single pass
    if (compiledData.empty())
    {
        auto shaderPass = std::make_unique<ShaderPass>();

        try
        {
            if (isCompute)
                compiler.CompileComputeShader(absoluteAssetPath.string().c_str(), ssf);
            else
                compiler.Compile(absoluteAssetPath.string().c_str(), ssf);
        }
        catch (std::exception e)
        {
            SPDLOG_ERROR("{}, {}", e.what(), absoluteAssetPath.string());
            return;
        }
        auto& includedFiles = compiler.GetIncludedFiles();
        includedFilesSet.insert(includedFiles.begin(), includedFiles.end());

        PassCompiledData passCompiledData;
        for (auto& iter : compiler.GetCompiledSpvs())
        {
            PassCompiledData::Feature group;
            group.mask = iter.first;
            group.compiledSpv = iter.second;
            passCompiledData.features.push_back(group);
        }

        passCompiledData.shaderPassName = compiler.GetName();
        passCompiledData.featureToBitmask = compiler.GetFeatureToBitMask();
        passCompiledData.shaderConfig = compiler.GetConfig()->ToJson();
        compiledData.push_back(passCompiledData);
    }
    meta["shaderFileLastWriteTime"] = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    meta["includedFiles"] = nlohmann::json::array_t();
    for (auto includedFile : includedFilesSet)
    {
        nlohmann::json::object_t includedFileJson;
        includedFileJson["path"] = includedFile.string();
        includedFileJson["lastWriteTime"] = std::filesystem::last_write_time(includedFile).time_since_epoch().count();
        meta["includedFiles"].push_back(includedFileJson);
    }

    meta["compiledShaderPasses"] = nlohmann::json::array_t();
    for (auto& c : compiledData)
    {
        nlohmann::json passj = nlohmann::json::object_t();
        passj["shaderPassName"] = c.shaderPassName;

        for (auto f : c.featureToBitmask)
        {
            passj["featureToBitmask"][f.first] = nlohmann::json::object_t();
            passj["featureToBitmask"][f.first]["shaderFeature"] = f.second.shaderFeature;
            passj["featureToBitmask"][f.first]["vertFeature"] = f.second.vertFeature;
            passj["featureToBitmask"][f.first]["fragFeature"] = f.second.fragFeature;
        }

        auto featuresJ = nlohmann::json::array_t();
        for (auto f : c.features)
        {
            nlohmann::json fj = nlohmann::json::object_t();

            fj["mask"]["shaderFeature"] = f.mask.shaderFeature;
            fj["mask"]["vertFeature"] = f.mask.vertFeature;
            fj["mask"]["fragFeature"] = f.mask.fragFeature;

            auto buildSpvInfoJson = [&binaryData](std::vector<uint32_t>& spv, std::vector<uint32_t>& unoptimizedSpv)
            {
                nlohmann::json j = nlohmann::json::object_t();
                size_t byteSize = spv.size() * sizeof(uint32_t);
                size_t offset = binaryData.size();
                j["size"] = byteSize;
                j["offset"] = offset;
                binaryData.resize(binaryData.size() + byteSize);
                memcpy(binaryData.data() + offset, spv.data(), byteSize);

                spirv_cross::CompilerReflection compilerReflection(
                    (const uint32_t*)unoptimizedSpv.data(),
                    unoptimizedSpv.size()
                );

                nlohmann::json jsonInfo = nlohmann::json::parse(compilerReflection.compile());
                j["reflection"] = jsonInfo;

                return j;
            };

            auto compiledSpvJson = nlohmann::json::object_t();
            if (!f.compiledSpv.vertSpv.empty())
                compiledSpvJson["vert"] = buildSpvInfoJson(f.compiledSpv.vertSpv, f.compiledSpv.vertSpv_noOp);

            if (!f.compiledSpv.fragSpv.empty())
                compiledSpvJson["frag"] = buildSpvInfoJson(f.compiledSpv.fragSpv, f.compiledSpv.fragSpv_noOp);

            if (!f.compiledSpv.compSpv.empty())
                compiledSpvJson["comp"] = buildSpvInfoJson(f.compiledSpv.compSpv, f.compiledSpv.compSpv_noOp);

            fj["compiledSpv"] = compiledSpvJson;

            featuresJ.push_back(fj);
        }

        passj["shaderConfig"] = c.shaderConfig;
        passj["features"] = featuresJ;

        meta["compiledShaderPasses"].push_back(passj);
    }

    std::string compiledSpvGUID = UUID().ToString();
    auto importName = fmt::format("{}_{}", "shader", compiledSpvGUID);
    meta["importedBinaryFileName"] = importName;

    auto importAssetPath = importDatabase->GetImportAssetPath(importName);
    std::fstream output;
    output.open(importAssetPath, std::ios_base::out | std::ios_base::binary);
    output.write((char*)binaryData.data(), binaryData.size());
}
void ShaderLoader::Load()
{
    nlohmann::json shaderPasses = meta["compiledShaderPasses"];
    bool isCompute = absoluteAssetPath.extension() == ".comp";
    std::ifstream input;
    std::vector<uint8_t> binaryData = importDatabase->ReadFile(meta["importedBinaryFileName"]);
    std::vector<std::unique_ptr<ShaderPass>> passes;

    for (auto passj : shaderPasses)
    {
        std::unique_ptr<ShaderPass> pass = std::make_unique<ShaderPass>();

        std::shared_ptr<Gfx::ShaderConfig> shaderConfig =
            std::make_shared<Gfx::ShaderConfig>(Gfx::ShaderConfig::FromJson(passj["shaderConfig"]));
        pass->name = passj["shaderPassName"];
        pass->cachedShaderProgram = nullptr;
        pass->globalShaderFeaturesHash = 0;

        for (auto& featureToBitmaskj : passj["featureToBitmask"].items())
        {
            ShaderFeatureBitmask bitmask;
            bitmask.shaderFeature = featureToBitmaskj.value()["shaderFeature"];
            bitmask.vertFeature = featureToBitmaskj.value()["vertFeature"];
            bitmask.fragFeature = featureToBitmaskj.value()["fragFeature"];

            pass->featureToBitmask[featureToBitmaskj.key()] = bitmask;
        }

        for (auto& featurej : passj["features"])
        {
            auto& maskj = featurej["mask"];
            ShaderFeatureBitmask bitmask;
            bitmask.shaderFeature = maskj["shaderFeature"];
            bitmask.vertFeature = maskj["vertFeature"];
            bitmask.fragFeature = maskj["fragFeature"];

            if (isCompute)
            {
                Gfx::ShaderProgramCreateInfo createInfo;
                size_t byteOffset = featurej["compiledSpv"]["comp"]["offset"];
                size_t byteSize = featurej["compiledSpv"]["comp"]["size"];
                std::vector<uint32_t> data(byteSize / sizeof(uint32_t));
                memcpy(data.data(), binaryData.data() + byteOffset, byteSize);
                createInfo.compSpv = data;
                createInfo.compReflection = featurej["compiledSpv"]["comp"]["reflection"];
                pass->shaderPrograms[bitmask] =
                    GetGfxDriver()->CreateShaderProgram(pass->name, shaderConfig, createInfo);
            }
            else
            {
                Gfx::ShaderProgramCreateInfo createInfo;
                size_t vertByteOffset = featurej["compiledSpv"]["vert"]["offset"];
                size_t vertByteSize = featurej["compiledSpv"]["vert"]["size"];
                std::vector<uint32_t> vertData(vertByteSize / sizeof(uint32_t));
                memcpy(vertData.data(), binaryData.data() + vertByteOffset, vertByteSize);
                createInfo.vertSpv = std::move(vertData);
                createInfo.vertReflection = featurej["compiledSpv"]["vert"]["reflection"];

                size_t fragByteOffset = featurej["compiledSpv"]["frag"]["offset"];
                size_t fragByteSize = featurej["compiledSpv"]["frag"]["size"];
                std::vector<uint32_t> fragData(fragByteSize / sizeof(uint32_t));
                memcpy(fragData.data(), binaryData.data() + fragByteOffset, fragByteSize);
                createInfo.fragSpv = std::move(fragData);
                createInfo.fragReflection = featurej["compiledSpv"]["frag"]["reflection"];

                pass->shaderPrograms[bitmask] =
                    GetGfxDriver()->CreateShaderProgram(pass->name, shaderConfig, createInfo);
            }
        }
        passes.push_back(std::move(pass));
    }

    std::unique_ptr<ShaderBase> shader;
    if (absoluteAssetPath.extension() == ".shad")
    {
        shader = std::make_unique<Shader>();
    }
    else if (absoluteAssetPath.extension() == ".comp")
    {
        shader = std::make_unique<ComputeShader>();
    }

    shader->SetShaderPasses(std::move(passes));
    asset = std::move(shader);
    //
    // shader->LoadFromFile(absoluteAssetPath.string().c_str());
}
