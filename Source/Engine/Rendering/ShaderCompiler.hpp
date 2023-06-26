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
    void Compile(const std::string& buf, bool debug)
    {
        std::stringstream f;
        f << buf;
        size_t bufSize = buf.size();

        std::stringstream yamlConfig = GetYAML(f);

        ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(yamlConfig.str()));

        config = MapShaderConfig(tree, name);

        includedTrack.clear();
        vertSpv =
            CompileShader("VERT", shaderc_vertex_shader, debug, "vertex shader", buf.c_str(), bufSize, includedTrack);

        fragSpv = CompileShader(
            "FRAG",
            shaderc_fragment_shader,
            debug,
            "fragment shader",
            buf.c_str(),
            bufSize,
            includedTrack
        );
    }

    const std::vector<uint32_t>& GetVertexSPV()
    {
        return vertSpv;
    }

    const std::vector<uint32_t>& GetFragSPV()
    {
        return fragSpv;
    }

    const std::string& GetName()
    {
        return name;
    }

    const Gfx::ShaderConfig& GetConfig()
    {
        return config;
    }

    void Clear()
    {
        vertSpv.clear();
        fragSpv.clear();
        includedTrack.clear();
    }

private:
    std::vector<uint32_t> vertSpv;
    std::vector<uint32_t> fragSpv;
    std::set<std::filesystem::path> includedTrack;
    Gfx::ShaderConfig config;
    std::string name;

    std::stringstream GetYAML(std::stringstream& f);

    // shaderStage: VERT for vertex shader, FRAG for fragment shader
    std::vector<uint32_t> CompileShader(
        const char* shaderStage,
        shaderc_shader_kind kind,
        bool debug,
        const char* debugName,
        const char* buf,
        int bufSize,
        std::set<std::filesystem::path>& includedTrack
    );

    static Gfx::ShaderConfig MapShaderConfig(ryml::Tree& tree, std::string& name);
};
} // namespace Engine
