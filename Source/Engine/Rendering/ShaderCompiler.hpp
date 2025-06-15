// #pragma once
// #include "GfxDriver/CompiledSpv.hpp"
// #include "GfxDriver/ShaderConfig.hpp"
// #include "Rendering/EnumStringMapping.hpp"
// #include "ShaderFeatureBitmask.hpp"
// #include <filesystem>
// #include <fmt/format.h>
// #include <fstream>
// #include <regex>
// #include <ryml.hpp>
// #include <ryml_std.hpp>
// #include <set>
// #include <shaderc/shaderc.h>
// #include <shaderc/shaderc.hpp>
// #include <sstream>
// #include <stack>
// #include <stdexcept>
// #include <unordered_map>
// #include "Libs/DynamicArray.hpp"
//
// class ShaderCompiler
// {
// public:
//     class CompileError : public std::logic_error
//     {
//         using std::logic_error::logic_error;
//     };
//
// private:
//     class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
//     {
//     private:
//         struct FileData
//         {
//         public:
//             DynamicArray<char> content;
//             std::string sourceName;
//         };
//
//         std::filesystem::path shaderRootPath = "Assets/Shaders/";
//         std::set<std::filesystem::path>* includedFiles = nullptr;
//
//     public:
//         ShaderIncluder(std::set<std::filesystem::path>* includedFileTrack) : includedFiles(includedFileTrack){};
//
//         virtual shaderc_include_result* GetInclude(
//             const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth
//         ) override;
//
//         // Handles shaderc_include_result_release_fn callbacks.
//         virtual void ReleaseInclude(shaderc_include_result* data) override
//         {
//             if (data->user_data != nullptr)
//                 delete (FileData*)data->user_data;
//             delete data;
//         }
//
//         virtual ~ShaderIncluder() = default;
//     };
//
// public:
//     const std::unordered_map<std::string, ShaderFeatureBitmask>& GetFeatureToBitMask()
//     {
//         return featureToBitMask;
//     }
//
//     // compile graphics shader
//     void Compile(const char* filepath, const std::string& buf);
//
//     // compile compute shader
//     void CompileComputeShader(const char* path, const std::string& buf);
//
//     const DynamicArray<uint32_t>& GetVertexSPV()
//     {
//         return compiledSpvs[0].vertSpv;
//     }
//
//     const DynamicArray<uint32_t>& GetFragSPV()
//     {
//         return compiledSpvs[0].fragSpv;
//     }
//
//     std::unordered_map<ShaderFeatureBitmask, Gfx::CompiledSpv>& GetCompiledSpvs()
//     {
//         return compiledSpvs;
//     }
//
//     const std::string& GetName()
//     {
//         return name;
//     }
//
//     std::shared_ptr<const Gfx::ShaderConfig> GetConfig()
//     {
//         return config;
//     }
//
//     // valid after Compile is called
//     const std::set<std::filesystem::path>& GetIncludedFiles() const
//     {
//         return includedTrack;
//     }
//
//     void Clear()
//     {
//         compiledSpvs.clear();
//         includedTrack.clear();
//     }
//
// private:
//     enum class ShaderFeatureBitmaskStage
//     {
//         ShaderGlobal,
//         Vert,
//         Frag
//     };
//     std::unordered_map<ShaderFeatureBitmask, Gfx::CompiledSpv> compiledSpvs{};
//
//     std::set<std::filesystem::path> includedTrack{};
//     std::shared_ptr<Gfx::ShaderConfig> config{};
//     std::string name;
//     // all feature bitmasks are stored in this variable including shader's global bitmask, per stage bitmasks
//     std::unordered_map<std::string, ShaderFeatureBitmask> featureToBitMask{};
//
//     std::stringstream GetYAML(std::stringstream& f);
//
//     // shaderStage: VERT for vertex shader, FRAG for fragment shader, COMP for compute shader(COMP or not doesn't really
//     // matter) actually matter)
//     void CompileShader(
//         const char* shaderStage,
//         shaderc_shader_kind kind,
//         bool debug,
//         const char* filepath,
//         const char* buf,
//         int bufSize,
//         std::set<std::filesystem::path>& includedTrack,
//         const DynamicArray<std::string>& features,
//         const DynamicArray<std::string>& stagefeatures,
//         DynamicArray<uint32_t>& unoptimized,
//         DynamicArray<uint32_t>& optimized
//     );
//     DynamicArray<DynamicArray<std::string>> FeatureToCombinations(const DynamicArray<DynamicArray<std::string>>&);
//     ShaderFeatureBitmask GenerateFeatureCombination(
//         const DynamicArray<std::string>& combs,
//         const std::unordered_map<std::string, ShaderFeatureBitmask>& featureToBitIndex
//     );
//     void FeaturesToBitmask(DynamicArray<DynamicArray<std::string>>& features, ShaderFeatureBitmaskStage shaderStage);
//     static DynamicArray<DynamicArray<std::string>> ExtractFeatures(ryml::NodeRef& root, ryml::csubstr featureName);
//     static Gfx::ShaderConfig MapShaderConfig(ryml::Tree& tree, std::string& name);
// };
