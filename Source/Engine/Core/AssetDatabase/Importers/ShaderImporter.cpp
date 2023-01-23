#include "ShaderImporter.hpp"
#include "Core/Graphics/Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Utils/EnumStringMapping.hpp"
#include "Utils/InternalAssets.hpp"
#if GAME_EDITOR
#include "Editor/ProjectManagement/ProjectManagement.hpp"
#endif
#include <filesystem>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <set>
#include <shaderc/shaderc.hpp>
#include <string>

using namespace Engine::Gfx;
using namespace Engine::Utils;

namespace Engine::Internal
{
class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
private:
    struct FileData
    {
    public:
        std::vector<char> content;
        std::string sourceName;
    };

    std::filesystem::path requestingSourceRelativeFullPath;
    std::set<std::filesystem::path>* includedFiles;

public:
    ShaderIncluder(std::set<std::filesystem::path>* includedFileTrack) : includedFiles(includedFileTrack){};

    virtual shaderc_include_result* GetInclude(const char* requested_source,
                                               shaderc_include_type type,
                                               const char* requesting_source,
                                               size_t include_depth) override
    {
        std::filesystem::path requestingSource(requesting_source);
        if (include_depth == 1)
        {
            requestingSourceRelativeFullPath = "Assets/Shaders/";
        }
        else
        {
            requestingSourceRelativeFullPath /= requestingSource.parent_path();
        }

        // if the we can't find the included file in the project, try find it in the internal assets
        std::filesystem::path relativePath = requestingSourceRelativeFullPath / std::filesystem::path(requested_source);
#if GAME_EDITOR
        if (!std::filesystem::exists(relativePath))
        {
            std::filesystem::path enginePath = Editor::ProjectManagement::GetInternalRootPath();
            relativePath = enginePath / relativePath;
        }
#endif

        std::fstream f;
        f.open(relativePath, std::ios::in);
        if (f.fail())
        {
            return new shaderc_include_result{"", 0, "Can't find requested shader file."};
        }
        if (includedFiles) includedFiles->insert(std::filesystem::absolute(relativePath));

        FileData* fileData = new FileData();
        fileData->sourceName = std::string(requested_source);
        fileData->content = std::vector<char>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());

        shaderc_include_result* result = new shaderc_include_result;
        result->content = fileData->content.data();
        result->content_length = fileData->content.size();
        result->source_name = fileData->sourceName.data();
        result->source_name_length = fileData->sourceName.size();
        result->user_data = fileData;
        return result;
    }

    // Handles shaderc_include_result_release_fn callbacks.
    virtual void ReleaseInclude(shaderc_include_result* data) override
    {
        if (data->user_data != nullptr) delete (FileData*)data->user_data;
        delete data;
    }

    virtual ~ShaderIncluder() = default;
};

static void CompileShader(const std::filesystem::path& path,
                          const std::filesystem::path& root,
                          const char* name,
                          const UUID& uuid,
                          shaderc_shader_kind kind,
                          shaderc::CompileOptions options);

static ShaderConfig MapShaderConfig(ryml::Tree& tree, std::string& name)
{
    ShaderConfig config;
    config.color.blendConstants[0] = 1.0;
    config.color.blendConstants[1] = 1.0;
    config.color.blendConstants[2] = 1.0;
    config.color.blendConstants[3] = 1.0;

    ryml::NodeRef root = tree.rootref();
    root["name"] >> name;
    root.get_if("interleaved", &config.vertexInterleaved);
    config.depth.boundTestEnable = false;

    if (root.has_child("blend"))
    {
        if (root["blend"].is_seq())
        {
            for (const ryml::NodeRef& iter : root["blend"])
            {
                if (iter.is_val())
                {
                    ColorBlendAttachmentState state;
                    std::string val;
                    iter >> val;

                    std::regex blendWithColorPattern("(\\w+)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)");
                    std::regex blendPattern("(\\w+)\\s+(\\w+)");
                    std::cmatch m;
                    if (std::regex_match(val.c_str(), m, blendWithColorPattern))
                    {
                        state.blendEnable = true;
                        std::string srcColorBlendFactor = m[1].str();
                        std::string srcAlphaBlendFactor = m[2].str();
                        std::string dstColorBlendFactor = m[3].str();
                        std::string dstAlphaBlendFactor = m[4].str();

                        state.srcColorBlendFactor = MapBlendFactor(srcColorBlendFactor);
                        state.srcAlphaBlendFactor = MapBlendFactor(srcAlphaBlendFactor);
                        state.dstColorBlendFactor = MapBlendFactor(dstColorBlendFactor);
                        state.dstAlphaBlendFactor = MapBlendFactor(dstAlphaBlendFactor);
                    }
                    else if (std::regex_match(val.c_str(), m, blendPattern))
                    {
                        state.blendEnable = true;
                        std::string srcAlphaBlendFactor = m[1].str();
                        std::string dstAlphaBlendFactor = m[2].str();
                        state.srcAlphaBlendFactor = MapBlendFactor(srcAlphaBlendFactor);
                        state.dstAlphaBlendFactor = MapBlendFactor(dstAlphaBlendFactor);
                        state.srcColorBlendFactor = state.srcAlphaBlendFactor;
                        state.dstColorBlendFactor = state.dstAlphaBlendFactor;

                        if (state.srcAlphaBlendFactor == Gfx::BlendFactor::One &&
                            state.dstAlphaBlendFactor == Gfx::BlendFactor::Zero)
                            state.blendEnable = false;
                    }

                    config.color.blends.push_back(state);
                }
            }
        }
    }

    if (root.has_child("blendOp"))
    {
        if (root["blendOp"].is_seq())
        {
            uint32_t i = 0;
            for (const ryml::NodeRef& iter : root["blendOp"])
            {
                if (i >= config.color.blends.size()) break;

                std::regex alphaOnly("(\\w+)");
                std::regex withColor("(\\w+)\\s+(\\w+)");

                std::string val;
                iter >> val;

                std::cmatch m;
                if (std::regex_match(val.c_str(), m, withColor))
                {
                    config.color.blends[i].colorBlendOp = MapBlendOp(m[1].str());
                    config.color.blends[i].alphaBlendOp = MapBlendOp(m[2].str());
                }
                else if (std::regex_match(val.c_str(), m, alphaOnly))
                {
                    config.color.blends[i].alphaBlendOp = MapBlendOp(m[1].str());
                    config.color.blends[i].colorBlendOp = config.color.blends[i].alphaBlendOp;
                }
            }
        }
    }

    if (root.has_child("cull"))
    {
        std::string val;
        auto cull = root["cull"];
        if (cull.is_keyval())
        {
            root["cull"] >> val;
            config.cullMode = MapCullMode(val);
        }
    }

    if (root.has_child("depth"))
    {
        std::string val;
        auto depth = root["depth"];
        depth.get_if("testEnable", &config.depth.testEnable);
        depth.get_if("writeEnable", &config.depth.writeEnable);
        depth.get_if("compOp", &val, std::string("always"));
        config.depth.compOp = MapCompareOp(val);
        depth.get_if("boundTestEnable", &config.depth.boundTestEnable);
        depth.get_if("minBounds", &config.depth.minBounds);
        depth.get_if("maxBounds", &config.depth.maxBounds);
    }

    if (root.has_child("stencil"))
    {
        std::string val;
        auto stencil = root["stencil"];
        stencil.get_if("testEnable", &config.stencil.testEnable);
        if (stencil.has_child("front"))
        {
            auto front = stencil["front"];
            front.get_if("failOp", &val, std::string("keep"));
            config.stencil.front.failOp = MapStencilOp(val);
            front.get_if("passOp", &val, std::string("keep"));
            config.stencil.front.passOp = MapStencilOp(val);
            front.get_if("depthFailOp", &val, std::string("keep"));
            config.stencil.front.depthFailOp = MapStencilOp(val);
            front.get_if("compareOp", &val, std::string("never"));
            config.stencil.front.compareOp = MapCompareOp(val);
            front.get_if("compareMask", &config.stencil.front.compareMask);
            front.get_if("writeMask", &config.stencil.front.writeMask);
            front.get_if("reference", &config.stencil.front.reference);
        }

        if (stencil.has_child("back"))
        {
            auto back = stencil["back"];
            back.get_if("failOp", &val, std::string("keep"));
            config.stencil.back.failOp = MapStencilOp(val);
            back.get_if("passOp", &val, std::string("keep"));
            config.stencil.back.passOp = MapStencilOp(val);
            back.get_if("depthFailOp", &val, std::string("keep"));
            config.stencil.back.depthFailOp = MapStencilOp(val);
            back.get_if("compareOp", &val, std::string("never"));
            config.stencil.back.compareOp = MapCompareOp(val);
            back.get_if("compareMask", &config.stencil.back.compareMask);
            back.get_if("writeMask", &config.stencil.back.writeMask);
            back.get_if("reference", &config.stencil.back.reference);
        }
    }

    return config;
}

bool ShaderImporter::NeedReimport(const std::filesystem::path& path,
                                  const std::filesystem::path& root,
                                  const UUID& uuid)
{
    auto outputDir = root / "Library" / uuid.ToString();

    if (!std::filesystem::exists(outputDir)) return true;

    if (GetLastWriteTime(outputDir / YAML_FileName) < GetLastWriteTime(path)) return true;

    std::ifstream dep(outputDir / DEP_FileName, std::ios::in);
    nlohmann::json depJ = nlohmann::json::parse(dep);

    for (auto& p : depJ)
    {
        std::filesystem::path path = p.value<std::string>("path", "");
        std::time_t lastWriteTime = p.value<std::time_t>("lastWriteTime", 0);
        std::time_t currWriteTime = GetLastWriteTime(path);
        if (!std::filesystem::exists(path) || (currWriteTime > lastWriteTime)) return true;
    }

    return false;
}

void ShaderImporter::Import(const std::filesystem::path& path,
                            const std::filesystem::path& root,
                            const nlohmann::json& json,
                            const UUID& rootUUID,
                            const std::unordered_map<std::string, UUID>& containedUUIDs)
{
    if (!std::filesystem::exists(path)) return;

    auto outputDir = root / "Library" / rootUUID.ToString();
    bool debug = true;

    if (!std::filesystem::exists(outputDir))
    {
        std::filesystem::create_directory(outputDir);
    }
    // Config
    std::fstream f;
    f.open(path, std::ios::in | std::ios::binary);
    if (!f.is_open() || !f.good()) return;

    f.seekg(0, std::ios::end);
    size_t fSize = std::filesystem::file_size(path);
    UniPtr<char> buf = UniPtr<char>(new char[fSize]);
    // shader Buffer
    f.seekg(0, std::ios::beg);
    f.read(buf.Get(), fSize);
    const int MAX_LENGTH = 1024;
    char* line = new char[MAX_LENGTH];
    std::stringstream yamlConfig;
    std::regex begin("\\s*#(if|ifdef)\\s+CONFIG\\s*");
    std::regex end("\\s*#endif\\s*");
    f.seekg(0, std::ios::beg);
    while (f.getline(line, MAX_LENGTH))
    { // find #if|ifdef CONFIG
        if (std::regex_match(line, begin))
        {
            while (f.getline(line, MAX_LENGTH))
            {
                if (!std::regex_match(line, end))
                {
                    yamlConfig << line << '\n';
                }
                else goto yamlEnd;
            }
        }
    }
yamlEnd:
    std::ofstream outConfig;
    outConfig.open(outputDir / YAML_FileName, std::ios::binary | std::ios::trunc | std::ios::out);
    outConfig.write(yamlConfig.str().c_str(), yamlConfig.str().size());
    std::filesystem::last_write_time(outputDir / YAML_FileName, std::filesystem::last_write_time(path));
    std::set<std::filesystem::path> includedTrack;

    // Vert
    shaderc::CompileOptions vertOption;
    vertOption.AddMacroDefinition("VERT", "1");
    vertOption.SetIncluder(std::make_unique<ShaderIncluder>(&includedTrack));
    if (debug)
    {
        vertOption.SetGenerateDebugInfo();
        vertOption.SetOptimizationLevel(shaderc_optimization_level_zero);
    }
    CompileShader(path, root, "vert.spv", rootUUID, shaderc_shader_kind::shaderc_vertex_shader, vertOption);

    // Frag
    shaderc::CompileOptions fragOption;
    fragOption.AddMacroDefinition("FRAG", "1");
    fragOption.SetIncluder(std::make_unique<ShaderIncluder>(&includedTrack));
    if (debug)
    {
        fragOption.SetGenerateDebugInfo();
        fragOption.SetOptimizationLevel(shaderc_optimization_level_zero);
    }
    CompileShader(path, root, "frag.spv", rootUUID, shaderc_shader_kind::shaderc_fragment_shader, fragOption);

    // write included track
    nlohmann::json dependencyTrack;
    int i = 0;
    for (auto& p : includedTrack)
    {
        nlohmann::json fileDep;
        fileDep["path"] = p.string();
        fileDep["lastWriteTime"] = GetLastWriteTime(p);
        dependencyTrack[i] = fileDep;
        i += 1;
    }
    std::ofstream depOut(outputDir / DEP_FileName, std::ios::trunc | std::ios::out);
    depOut << dependencyTrack.dump();
}

static std::vector<char> ReadSpvFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::in);
    std::vector<char> data{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return data;
}

UniPtr<AssetObject> ShaderImporter::Load(const std::filesystem::path& root,
                                         ReferenceResolver& refResolver,
                                         const UUID& uuid)
{
    auto inputDir = root / "Library" / uuid.ToString();

    if (std::filesystem::exists(inputDir))
    {
        // config
        std::ifstream configFile(inputDir / YAML_FileName, std::ios::binary | std::ios::in);
        std::string yamlConfig{std::istreambuf_iterator<char>(configFile), std::istreambuf_iterator<char>()};

        ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlConfig.c_str()));
        std::string name;
        ShaderConfig config = MapShaderConfig(tree, name);

        auto vert = ReadSpvFile(inputDir / VERT_FileName);
        auto frag = ReadSpvFile(inputDir / FRAG_FileName);

        auto shaderProgram = Gfx::GfxDriver::Instance()->CreateShaderProgram(name,
                                                                             &config,
                                                                             (unsigned char*)&vert.front(),
                                                                             vert.size(),
                                                                             (unsigned char*)&frag.front(),
                                                                             frag.size());
        UniPtr<Shader> shader = MakeUnique<Shader>(name, std::move(shaderProgram), uuid);
        return shader;
    }

    return nullptr;
}

void CompileShader(const std::filesystem::path& path,
                   const std::filesystem::path& root,
                   const char* name,
                   const UUID& uuid,
                   shaderc_shader_kind kind,
                   shaderc::CompileOptions options)
{
    if (std::filesystem::exists(path))
    {
        // read file
        std::fstream f;
        f.open(path, std::ios::in | std::ios::binary);
        if (!f.is_open() || !f.good()) return;

        f.seekg(0, std::ios::end);
        size_t fSize = std::filesystem::file_size(path);
        UniPtr<char> buf = UniPtr<char>(new char[fSize]);
        // shader Buffer
        f.seekg(0, std::ios::beg);
        f.read(buf.Get(), fSize);

        shaderc::Compiler compiler;
        auto outputPath = root / "Library" / uuid.ToString();
        if (!std::filesystem::exists(outputPath))
        {
            std::filesystem::create_directory(outputPath);
        }

        std::ofstream output;
        output.open(outputPath / name, std::ios::trunc | std::ios::out | std::ios::binary);

        if (output.is_open() && output.good())
        {
            auto compiled = compiler.CompileGlslToSpv((const char*)buf.Get(),
                                                      fSize,
                                                      kind,
                                                      path.relative_path().string().c_str(),
                                                      options);
            if (compiled.GetNumErrors() > 0)
            {
                SPDLOG_ERROR("Shader compiled error [UUID: {}], [name: {}]: {}",
                             uuid.ToString().c_str(),
                             name,
                             compiled.GetErrorMessage().c_str());
            }
            output.write((const char*)compiled.begin(),
                         sizeof(shaderc::SpvCompilationResult::element_type) * (compiled.end() - compiled.begin()));
        }
    }
}

const std::type_info& ShaderImporter::GetObjectType() { return typeid(Shader); }
} // namespace Engine::Internal
