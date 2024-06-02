#pragma once
#include "GfxDriver/CompiledSpv.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Utils/EnumStringMapping.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <regex>
#include <ryml.hpp>
#include <ryml_std.hpp>
#include <set>
#include <shaderc/shaderc.h>
#include <shaderc/shaderc.hpp>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <vector>

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

        std::filesystem::path shaderRootPath = "Assets/Shaders/";
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

    // compile graphics shader
    void Compile(const char* filepath, const std::string& buf);

    // compile compute shader
    void CompileComputeShader(const char* path, const std::string& buf);

    const std::vector<uint32_t>& GetVertexSPV()
    {
        return compiledSpvs[0].vertSpv;
    }

    const std::vector<uint32_t>& GetFragSPV()
    {
        return compiledSpvs[0].fragSpv;
    }

    std::unordered_map<uint64_t, Gfx::CompiledSpv>& GetCompiledSpvs()
    {
        return compiledSpvs;
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
    std::unordered_map<uint64_t, Gfx::CompiledSpv> compiledSpvs;

    std::set<std::filesystem::path> includedTrack;
    std::shared_ptr<Gfx::ShaderConfig> config;
    std::string name;
    std::unordered_map<std::string, uint64_t> featureToBitMask;

    std::stringstream GetYAML(std::stringstream& f);

    // shaderStage: VERT for vertex shader, FRAG for fragment shader, COMP for compute shader(COMP or not doesn't really
    // matter) actually matter)
    void CompileShader(
        const char* shaderStage,
        shaderc_shader_kind kind,
        bool debug,
        const char* filepath,
        const char* buf,
        int bufSize,
        std::set<std::filesystem::path>& includedTrack,
        const std::vector<std::string>& features,
        std::vector<uint32_t>& unoptimized,
        std::vector<uint32_t>& optimized
    );
    std::vector<std::vector<std::string>> FeatureToCombinations(const std::vector<std::vector<std::string>>&);
    uint64_t GenerateFeatureCombination(
        const std::vector<std::string>& combs, const std::unordered_map<std::string, uint64_t>& featureToBitIndex
    );
    static Gfx::ShaderConfig MapShaderConfig(ryml::Tree& tree, std::string& name);
};
