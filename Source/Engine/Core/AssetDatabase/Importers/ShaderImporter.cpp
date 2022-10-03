#include "ShaderImporter.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Core/Graphics/Shader.hpp"
#include "Utils/EnumStringMapping.hpp"
#include "Utils/InternalAssets.hpp"
#include <fstream>
#include <regex>
#include <sstream>
#include <ryml_std.hpp>
#include <ryml.hpp>
#include <filesystem>
#include <shaderc/shaderc.hpp>

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

        public:
            virtual shaderc_include_result* GetInclude(
                    const char* requested_source,
                    shaderc_include_type type,
                    const char* requesting_source,
                    size_t include_depth) override
            {
                char shaderPath[1024] ="Assets/Shaders/";
                strcat(shaderPath, requested_source);

                // if the we can't find the included file in the project, try find it in the internal assets
#if GAME_EDITOR
                if (!std::filesystem::exists(shaderPath))
                {
                    std::fstream internalf;
                    shaderPath[0] = '\0';
                    strcpy(shaderPath, Utils::InternalAssets::GetInternalAssetPath().string().c_str());
                    strcat(shaderPath, "/Assets/Shaders/");
                    strcat(shaderPath, requested_source);
#endif
                }

                std::fstream f;
                f.open(shaderPath, std::ios::in);
                if (f.fail())
                {
                    return new shaderc_include_result{"", 0, "Can't find requested shader file."};
                }

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


    static ShaderConfig MapShaderConfig(ryml::Tree& tree, std::string& name)
    {
        ShaderConfig config;
        config.color.blendConstants[0] = 1.0;
        config.color.blendConstants[1] = 1.0;
        config.color.blendConstants[2] = 1.0;
        config.color.blendConstants[3] = 1.0;

        ryml::NodeRef root = tree.rootref();
        root["name"] >> name;
        root["interleaved"] >> config.vertexInterleaved;
        config.depth.boundTestEnable = false;

        if (root.has_child("blend"))
        {
            if (root["blend"].is_seq())
            {
                for(const ryml::NodeRef& iter : root["blend"])
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
                                state.dstAlphaBlendFactor == Gfx::BlendFactor::Zero) state.blendEnable = false;
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
                for(const ryml::NodeRef& iter : root["blendOp"])
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

    UniPtr<AssetObject> ShaderImporter::Import(
            const std::filesystem::path& path,
            ReferenceResolver& refResolver,
            const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs)
    {
        // read file
        std::fstream f;
        f.open(path, std::ios::in | std::ios::binary);
        if (!f.is_open() || !f.good()) return nullptr;

        f.seekg(0, std::ios::end);
        SPDLOG_INFO("{}", path.string());
        size_t fSize = std::filesystem::file_size(path);
        UniPtr<char> buf = UniPtr<char>(new char[fSize]);
        // shader Buffer
        f.seekg(0, std::ios::beg);
        f.read(buf.Get(), fSize);

        // Vert
        shaderc::Compiler compiler;
        shaderc::CompileOptions vertOption;
        vertOption.AddMacroDefinition("VERT", "1");
        vertOption.SetIncluder(std::make_unique<ShaderIncluder>());
        shaderc::SpvCompilationResult vertResult = compiler.CompileGlslToSpv((const char*)buf.Get(), fSize, shaderc_shader_kind::shaderc_vertex_shader, path.relative_path().string().c_str(), vertOption);
        if (vertResult.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success)
        {
            SPDLOG_ERROR("Failed to generate shader at {}, reason: {}", (const char*)path.c_str(), vertResult.GetErrorMessage().c_str());
            return nullptr;
        }

        // Frag
        shaderc::CompileOptions fragOption;
        fragOption.AddMacroDefinition("FRAG", "1");
        fragOption.SetIncluder(std::make_unique<ShaderIncluder>());
        shaderc::SpvCompilationResult fragResult = compiler.CompileGlslToSpv((const char*)buf.Get(), fSize, shaderc_shader_kind::shaderc_fragment_shader, path.relative_path().string().c_str(), fragOption);
        if (fragResult.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success)
        {
            SPDLOG_ERROR("Failed to generate shader at {}, reason: {}", (const char*)path.c_str(), fragResult.GetErrorMessage().c_str());
            return nullptr;
        }


        // config
        // parse input first
        const int MAX_LENGTH = 1024;
        char* line = new char[MAX_LENGTH];
        std::stringstream yamlConfig;
        std::regex begin("\\s*#(if|ifdef)\\s+CONFIG\\s*");
        std::regex end("\\s*#endif\\s*");
        f.seekg(0, std::ios::beg);
        while (f.getline(line, MAX_LENGTH)) { // find #if|ifdef CONFIG
            if (std::regex_match(line, begin))
            {
                while (f.getline(line, MAX_LENGTH)) {
                    if (!std::regex_match(line, end))
                    {
                        yamlConfig << line << '\n';
                    }
                    else
                        goto yamlEnd;
                }
            }
        }
yamlEnd:
        f.close();
        ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlConfig.str()));
        std::string name;
        ShaderConfig config = MapShaderConfig(tree, name);

        uint32_t vertSize = sizeof(shaderc::SpvCompilationResult::element_type) * (vertResult.end() - vertResult.begin());
        uint32_t fragSize = sizeof(shaderc::SpvCompilationResult::element_type) * (fragResult.end() - fragResult.begin());

        auto shaderProgram = Gfx::GfxDriver::Instance()->CreateShaderProgram(name,
                &config,
                (unsigned char*)vertResult.begin(),
                vertSize,
                (unsigned char*)fragResult.begin(),
                fragSize);
        UniPtr<Shader> shader = MakeUnique<Shader>(name, std::move(shaderProgram));
        return shader;
    }
}
