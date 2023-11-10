#pragma once
#include "GfxDriver/ShaderConfig.hpp"
#include "Utils/EnumStringMapping.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <set>
#include <shaderc/shaderc.hpp>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace Engine
{
class ShaderCompiler
{
public:
    class CompileError : public std::logic_error
    {
        using std::logic_error::logic_error;
    };

private:
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

        virtual shaderc_include_result* GetInclude(
            const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth
        ) override;

        // Handles shaderc_include_result_release_fn callbacks.
        virtual void ReleaseInclude(shaderc_include_result* data) override
        {
            if (data->user_data != nullptr)
                delete (FileData*)data->user_data;
            delete data;
        }

        virtual ~ShaderIncluder() = default;
    };

public:
    const std::unordered_map<std::string, uint64_t>& GetFeatureToBitMask()
    {
        return featureToBitMask;
    }

    void Compile(const std::string& buf, bool debug)
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

            CompiledSpv compiledSpv;

            compiledSpv.vertSpv = CompileShader(
                "VERT",
                shaderc_vertex_shader,
                debug,
                "vertex shader",
                buf.c_str(),
                bufSize,
                includedTrack,
                c
            );

            compiledSpv.fragSpv = CompileShader(
                "FRAG",
                shaderc_fragment_shader,
                debug,
                "fragment shader",
                buf.c_str(),
                bufSize,
                includedTrack,
                c
            );

            compiledSpvs[featureCombination] = std::move(compiledSpv);
        }
    }

    const std::vector<uint32_t>& GetVertexSPV()
    {
        return compiledSpvs[0].vertSpv;
    }

    const std::vector<uint32_t>& GetFragSPV()
    {
        return compiledSpvs[0].fragSpv;
    }

    const std::string& GetName()
    {
        return name;
    }

    std::shared_ptr<const Gfx::ShaderConfig> GetConfig()
    {
        return config;
    }

    void Clear()
    {
        compiledSpvs.clear();
        includedTrack.clear();
    }

private:
    struct CompiledSpv
    {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };
    std::unordered_map<uint64_t, CompiledSpv> compiledSpvs;

    std::set<std::filesystem::path> includedTrack;
    std::shared_ptr<Gfx::ShaderConfig> config;
    std::string name;
    std::unordered_map<std::string, uint64_t> featureToBitMask;

    std::stringstream GetYAML(std::stringstream& f);

    // shaderStage: VERT for vertex shader, FRAG for fragment shader
    std::vector<uint32_t> CompileShader(
        const char* shaderStage,
        shaderc_shader_kind kind,
        bool debug,
        const char* debugName,
        const char* buf,
        int bufSize,
        std::set<std::filesystem::path>& includedTrack,
        const std::vector<std::string>& features
    );
    std::vector<std::vector<std::string>> FeatureToCombinations(const std::vector<std::vector<std::string>>&);
    uint64_t GenerateFeatureCombination(
        const std::vector<std::string>& combs, const std::unordered_map<std::string, uint64_t>& featureToBitIndex
    );
    static Gfx::ShaderConfig MapShaderConfig(ryml::Tree& tree, std::string& name);
};
} // namespace Engine
