#include "ShaderCompiler.hpp"

namespace Engine
{
shaderc_include_result* ShaderCompiler::ShaderIncluder::GetInclude(
    const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth
)
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

    // if the we can't find the included file in the project, try find it in
    // the internal assets
    std::filesystem::path relativePath = requestingSourceRelativeFullPath / std::filesystem::path(requested_source);
    // #if GAME_EDITOR
    //             if (!std::filesystem::exists(relativePath))
    //             {
    //                 std::filesystem::path enginePath =
    //                 Editor::ProjectManagement::GetInternalRootPath();
    //                 relativePath = enginePath / relativePath;
    //             }
    // #endif

    std::fstream f;
    f.open(relativePath, std::ios::in);
    if (f.fail())
    {
        return new shaderc_include_result{"", 0, "Can't find requested shader file."};
    }
    if (includedFiles)
        includedFiles->insert(std::filesystem::absolute(relativePath));

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
std::vector<uint32_t> ShaderCompiler::CompileShader(
    const char* shaderStage,
    shaderc_shader_kind kind,
    bool debug,
    const char* debugName,
    const char* buf,
    int bufSize,
    std::set<std::filesystem::path>& includedTrack
)
{
    shaderc::CompileOptions option;
    option.AddMacroDefinition(shaderStage, "1");
    option.SetIncluder(std::make_unique<ShaderIncluder>(&includedTrack));
    if (debug)
    {
        option.SetGenerateDebugInfo();
        option.SetOptimizationLevel(shaderc_optimization_level_zero);
    }
    shaderc::Compiler compiler;
    auto compiled = compiler.CompileGlslToSpv((const char*)buf, bufSize, kind, debugName, option);
    if (compiled.GetNumErrors() > 0)
    {
        auto msg = fmt::format("Shader[{}] failed: {}", name, compiled.GetErrorMessage().c_str());
        throw CompileError(msg);
    }
    return std::vector<uint32_t>(compiled.begin(), compiled.end());
}

Gfx::ShaderConfig ShaderCompiler::MapShaderConfig(ryml::Tree& tree, std::string& name)
{
    Gfx::ShaderConfig config;
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
                    Gfx::ColorBlendAttachmentState state;
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

                        state.srcColorBlendFactor = Utils::MapBlendFactor(srcColorBlendFactor);
                        state.srcAlphaBlendFactor = Utils::MapBlendFactor(srcAlphaBlendFactor);
                        state.dstColorBlendFactor = Utils::MapBlendFactor(dstColorBlendFactor);
                        state.dstAlphaBlendFactor = Utils::MapBlendFactor(dstAlphaBlendFactor);
                    }
                    else if (std::regex_match(val.c_str(), m, blendPattern))
                    {
                        state.blendEnable = true;
                        std::string srcAlphaBlendFactor = m[1].str();
                        std::string dstAlphaBlendFactor = m[2].str();
                        state.srcAlphaBlendFactor = Utils::MapBlendFactor(srcAlphaBlendFactor);
                        state.dstAlphaBlendFactor = Utils::MapBlendFactor(dstAlphaBlendFactor);
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
                if (i >= config.color.blends.size())
                    break;

                std::regex alphaOnly("(\\w+)");
                std::regex withColor("(\\w+)\\s+(\\w+)");

                std::string val;
                iter >> val;

                std::cmatch m;
                if (std::regex_match(val.c_str(), m, withColor))
                {
                    config.color.blends[i].colorBlendOp = Utils::MapBlendOp(m[1].str());
                    config.color.blends[i].alphaBlendOp = Utils::MapBlendOp(m[2].str());
                }
                else if (std::regex_match(val.c_str(), m, alphaOnly))
                {
                    config.color.blends[i].alphaBlendOp = Utils::MapBlendOp(m[1].str());
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
            config.cullMode = Utils::MapCullMode(val);
        }
    }

    if (root.has_child("depth"))
    {
        std::string val;
        auto depth = root["depth"];
        depth.get_if("testEnable", &config.depth.testEnable);
        depth.get_if("writeEnable", &config.depth.writeEnable);
        depth.get_if("compOp", &val, std::string("always"));
        config.depth.compOp = Utils::MapCompareOp(val);
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
            config.stencil.front.failOp = Utils::MapStencilOp(val);
            front.get_if("passOp", &val, std::string("keep"));
            config.stencil.front.passOp = Utils::MapStencilOp(val);
            front.get_if("depthFailOp", &val, std::string("keep"));
            config.stencil.front.depthFailOp = Utils::MapStencilOp(val);
            front.get_if("compareOp", &val, std::string("never"));
            config.stencil.front.compareOp = Utils::MapCompareOp(val);
            front.get_if("compareMask", &config.stencil.front.compareMask);
            front.get_if("writeMask", &config.stencil.front.writeMask);
            front.get_if("reference", &config.stencil.front.reference);
        }

        if (stencil.has_child("back"))
        {
            auto back = stencil["back"];
            back.get_if("failOp", &val, std::string("keep"));
            config.stencil.back.failOp = Utils::MapStencilOp(val);
            back.get_if("passOp", &val, std::string("keep"));
            config.stencil.back.passOp = Utils::MapStencilOp(val);
            back.get_if("depthFailOp", &val, std::string("keep"));
            config.stencil.back.depthFailOp = Utils::MapStencilOp(val);
            back.get_if("compareOp", &val, std::string("never"));
            config.stencil.back.compareOp = Utils::MapCompareOp(val);
            back.get_if("compareMask", &config.stencil.back.compareMask);
            back.get_if("writeMask", &config.stencil.back.writeMask);
            back.get_if("reference", &config.stencil.back.reference);
        }
    }

    return config;
}

std::stringstream ShaderCompiler::GetYAML(std::stringstream& f)
{
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
                else
                    goto yamlEnd;
            }
        }
    }
yamlEnd:
    return yamlConfig;
}
} // namespace Engine
