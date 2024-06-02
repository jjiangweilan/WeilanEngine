#include "ShaderCompiler.hpp"
#include <spdlog/spdlog.h>
shaderc_include_result* ShaderCompiler::ShaderIncluder::GetInclude(
    const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t includeDepth
)
{
    std::filesystem::path requestedSource(requested_source);
    std::filesystem::path requestingSource(requesting_source);

    // test relative path to requestingSource firest
    auto finalPath = requestingSource.parent_path() / requestedSource;
    if (!std::filesystem::exists(finalPath))
    {
        // if relative path not working, try final it as relative to shader root
        finalPath = shaderRootPath / requestedSource;
    }

    std::fstream f;
    f.open(finalPath, std::ios::in);
    if (f.fail())
    {
        return new shaderc_include_result{"", 0, "Can't find requested shader file."};
    }
    if (includedFiles)
        includedFiles->insert(std::filesystem::absolute(finalPath));

    FileData* fileData = new FileData();
    fileData->sourceName = std::string(finalPath.string());
    fileData->content = std::vector<char>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());

    shaderc_include_result* result = new shaderc_include_result;
    result->content = fileData->content.data();
    result->content_length = fileData->content.size();
    result->source_name = fileData->sourceName.data();
    result->source_name_length = fileData->sourceName.size();
    result->user_data = fileData;
    return result;
}

std::vector<std::vector<std::string>> MakeCombination(
    std::vector<std::vector<std::string>> first, std::vector<std::vector<std::string>> second
)
{
    std::vector<std::vector<std::string>> comb{};
    for (int i = 0; i < first.size(); i++)
    {
        for (int j = 0; j < second.size(); j++)
        {
            std::vector<std::string>& f = first[i];
            std::vector<std::string>& s = second[j];

            std::vector<std::string> n(f.begin(), f.end());
            n.insert(n.end(), s.begin(), s.end());

            comb.push_back(std::move(n));
        }
    }

    return comb;
}

std::vector<std::vector<std::string>> ShaderCompiler::FeatureToCombinations(
    const std::vector<std::vector<std::string>>& features
)
{
    if (features.empty())
        return {};

    std::vector<std::vector<std::string>> combs{};
    std::vector<std::vector<std::vector<std::string>>> fs{};

    // vectorize
    for (auto& v : features)
    {
        std::vector<std::vector<std::string>> vf{};
        for (auto& s : v)
        {
            std::vector<std::string> singleFeature{};
            singleFeature.push_back(s);
            vf.push_back(std::move(singleFeature));
        }

        fs.push_back(vf);
    }

    // make combinations
    if (!fs.empty())
    {
        combs = fs[0];
        for (int i = 1; i < features.size(); ++i)
        {
            combs = MakeCombination(combs, fs[i]);
        }
    }

    return combs;
}
void ShaderCompiler::CompileShader(
    const char* shaderStage,
    shaderc_shader_kind kind,
    bool debug,
    const char* filepath,
    const char* buf,
    int bufSize,
    std::set<std::filesystem::path>& includedTrack,
    const std::vector<std::string>& features,
    std::vector<uint32_t>& optimized,
    std::vector<uint32_t>& unoptimized
)
{
    shaderc::CompileOptions option;
    // https://github.com/google/shaderc/commit/ca4c38cbc8137fba6fc3ddbf0d95362a04612fa2
    option.AddMacroDefinition(shaderStage, "1");
    for (auto& f : features)
    {
        option.AddMacroDefinition(f, "1");
    }
    auto includer = std::make_unique<ShaderIncluder>(&includedTrack);
    option.SetIncluder(std::move(includer));

    // currently settings for textures are derived from binding name but if we use optimization, the name will be
    // replaced causing failure to generate correct ShaderInfo.
    // We can move the setting from binding name to CONFIG section
    option.SetAutoBindUniforms(true);
    option.SetPreserveBindings(false);

    shaderc::Compiler compiler;
    option.SetOptimizationLevel(shaderc_optimization_level_performance);
    auto optimizedCompiled = compiler.CompileGlslToSpv((const char*)buf, bufSize, kind, filepath, option);
    if (optimizedCompiled.GetNumErrors() > 0)
    {
        auto msg = fmt::format("Shader[{}] failed: {}", name, optimizedCompiled.GetErrorMessage().c_str());
        throw CompileError(msg);
    }
    optimized = std::vector<uint32_t>(optimizedCompiled.begin(), optimizedCompiled.end());

    if (debug)
    {
        option.SetGenerateDebugInfo();
    }
    option.SetOptimizationLevel(shaderc_optimization_level_zero);
    auto unoptimizedCompiled = compiler.CompileGlslToSpv((const char*)buf, bufSize, kind, filepath, option);
    if (unoptimizedCompiled.GetNumErrors() > 0)
    {
        auto msg = fmt::format("Shader[{}] failed: {}", name, unoptimizedCompiled.GetErrorMessage().c_str());
        throw CompileError(msg);
    }
    unoptimized = std::vector<uint32_t>(unoptimizedCompiled.begin(), unoptimizedCompiled.end());
}

Gfx::ShaderConfig ShaderCompiler::MapShaderConfig(ryml::Tree& tree, std::string& name)
{
    Gfx::ShaderConfig config;
    config.color.blendConstants[0] = 1.0;
    config.color.blendConstants[1] = 1.0;
    config.color.blendConstants[2] = 1.0;
    config.color.blendConstants[3] = 1.0;

    ryml::NodeRef root = tree.rootref();
    if (root.empty())
        return config;

    root.get_if("name", &name);
    root.get_if("interleaved", &config.vertexInterleaved);
    root.get_if("debug", &config.debug);
    config.depth.boundTestEnable = false;

    if (root.has_child("input"))
    {
        if (root["input"].is_map())
        {
            for (const ryml::NodeRef& iter : root["input"])
            {
                std::string inputName(iter.key().begin(), iter.key().size());
                if (iter.has_child("baseTypeSize"))
                {
                    int size = 0;
                    iter.get_if("baseTypeSize", &size);
                    config.shaderInfoInputBaseTypeSizeOverride[inputName] = size;
                }
            }
        }
    }

    if (root.has_child("mask"))
    {

        if (root["mask"].is_seq()) // mask for multiple render target, to be implmented
        {
            throw std::runtime_error("not implmented");
            Gfx::ColorBlendAttachmentState state;
            config.color.blends.push_back(state);
        }
        else // mask for the first render target
        {
            std::string val;
            root["mask"] >> val;
            Gfx::ColorBlendAttachmentState state{};
            state.colorWriteMask = Utils::MapColorMask(val);
            config.color.blends.push_back(state);
        }
    }

    if (root.has_child("blend"))
    {
        if (root["blend"].is_seq())
        {
            int i = 0;
            for (const ryml::NodeRef& iter : root["blend"])
            {
                if (iter.is_val())
                {
                    Gfx::ColorBlendAttachmentState* state;
                    if (i < config.color.blends.size())
                    {
                        state = &config.color.blends[i];
                    }
                    else
                    {
                        config.color.blends.push_back(Gfx::ColorBlendAttachmentState{});
                        state = &config.color.blends.back();
                    }

                    std::string val;
                    iter >> val;

                    std::regex blendWithColorPattern("(\\w+)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)");
                    std::regex blendPattern("(\\w+)\\s+(\\w+)");
                    std::cmatch m;
                    if (std::regex_match(val.c_str(), m, blendWithColorPattern))
                    {
                        state->blendEnable = true;
                        std::string srcColorBlendFactor = m[1].str();
                        std::string srcAlphaBlendFactor = m[2].str();
                        std::string dstColorBlendFactor = m[3].str();
                        std::string dstAlphaBlendFactor = m[4].str();

                        state->srcColorBlendFactor = Utils::MapBlendFactor(srcColorBlendFactor);
                        state->srcAlphaBlendFactor = Utils::MapBlendFactor(srcAlphaBlendFactor);
                        state->dstColorBlendFactor = Utils::MapBlendFactor(dstColorBlendFactor);
                        state->dstAlphaBlendFactor = Utils::MapBlendFactor(dstAlphaBlendFactor);
                    }
                    else if (std::regex_match(val.c_str(), m, blendPattern))
                    {
                        state->blendEnable = true;
                        std::string srcAlphaBlendFactor = m[1].str();
                        std::string dstAlphaBlendFactor = m[2].str();
                        state->srcAlphaBlendFactor = Utils::MapBlendFactor(srcAlphaBlendFactor);
                        state->dstAlphaBlendFactor = Utils::MapBlendFactor(dstAlphaBlendFactor);
                        state->srcColorBlendFactor = state->srcAlphaBlendFactor;
                        state->dstColorBlendFactor = state->dstAlphaBlendFactor;

                        if (state->srcAlphaBlendFactor == Gfx::BlendFactor::One &&
                            state->dstAlphaBlendFactor == Gfx::BlendFactor::Zero)
                            state->blendEnable = false;
                    }
                }
                i++;
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

    if (root.has_child("features"))
    {
        auto features = root["features"];
        std::vector<std::vector<std::string>> featuresVal;
        if (features.is_seq())
        {
            for (auto fs : features)
            {
                std::vector<std::string> fval;
                if (fs.is_seq())
                {
                    for (const auto& f : fs)
                    {
                        std::string val;
                        f >> val;
                        fval.push_back(std::move(val));
                    }
                }
                featuresVal.push_back(std::move(fval));
            }
        }
        config.features = std::move(featuresVal);
    }
    else
        config.features = {};

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

uint64_t ShaderCompiler::GenerateFeatureCombination(
    const std::vector<std::string>& combs, const std::unordered_map<std::string, uint64_t>& featureToBitIndex
)
{
    uint64_t featureCombination = 0;
    for (auto& c : combs)
    {
        uint64_t bitMask = featureToBitIndex.at(c);
        featureCombination |= bitMask;
    }

    return featureCombination;
}

void ShaderCompiler::CompileComputeShader(const char* filepath, const std::string& buf)
{
    std::stringstream f;
    f << buf;
    size_t bufSize = buf.size();

    std::stringstream yamlConfig = GetYAML(f);

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlConfig.str()));

    config = std::make_shared<Gfx::ShaderConfig>(MapShaderConfig(tree, name));

    auto featureCombs = FeatureToCombinations(config->features);
    int bitIndex = 0;
    featureToBitMask.clear();
    const uint64_t one = 1;
    for (auto& fs : config->features)
    {
        for (int i = 0; i < fs.size(); ++i)
        {
            if (i == 0)
            {
                featureToBitMask[fs[i]] = 0;
            }
            else
            {
                featureToBitMask[fs[i]] = one << bitIndex;
                bitIndex += 1;
            }
        }
    }

    if (featureToBitMask.size() > 64)
    {
        throw CompileError("shader can't support more than 64 features");
    }

    includedTrack.clear();

    if (featureCombs.empty())
    {
        featureCombs.push_back({});
    }

    for (auto& c : featureCombs)
    {
        uint64_t featureCombination = GenerateFeatureCombination(c, featureToBitMask);

        Gfx::CompiledSpv compiledSpv;

        CompileShader(
            "COMP",
            shaderc_compute_shader,
            config->debug,
            filepath,
            buf.c_str(),
            bufSize,
            includedTrack,
            c,
            compiledSpv.compSpv,
            compiledSpv.compSpv_noOp
        );

        compiledSpvs[featureCombination] = std::move(compiledSpv);
    }
}

void ShaderCompiler::Compile(const char* filepath, const std::string& buf)
{
    std::stringstream f;
    f << buf;
    size_t bufSize = buf.size();

    std::stringstream yamlConfig = GetYAML(f);

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlConfig.str()));

    config = std::make_shared<Gfx::ShaderConfig>(MapShaderConfig(tree, name));

    auto featureCombs = FeatureToCombinations(config->features);
    int bitIndex = 0;
    featureToBitMask.clear();
    const uint64_t one = 1;
    for (auto& fs : config->features)
    {
        for (int i = 0; i < fs.size(); ++i)
        {
            if (i == 0)
            {
                featureToBitMask[fs[i]] = 0;
            }
            else
            {
                featureToBitMask[fs[i]] = one << bitIndex;
                bitIndex += 1;
            }
        }
    }

    if (featureToBitMask.size() > 64)
    {
        throw CompileError("shader can't support more than 64 features");
    }

    includedTrack.clear();

    if (featureCombs.empty())
    {
        featureCombs.push_back({});
    }

    for (auto& c : featureCombs)
    {
        uint64_t featureCombination = GenerateFeatureCombination(c, featureToBitMask);

        Gfx::CompiledSpv compiledSpv;

        CompileShader(
            "VERT",
            shaderc_vertex_shader,
            config->debug,
            filepath,
            buf.c_str(),
            bufSize,
            includedTrack,
            c,
            compiledSpv.vertSpv,
            compiledSpv.vertSpv_noOp
        );

        CompileShader(
            "FRAG",
            shaderc_fragment_shader,
            config->debug,
            filepath,
            buf.c_str(),
            bufSize,
            includedTrack,
            c,
            compiledSpv.fragSpv,
            compiledSpv.fragSpv_noOp
        );

        compiledSpvs[featureCombination] = std::move(compiledSpv);
    }
}
