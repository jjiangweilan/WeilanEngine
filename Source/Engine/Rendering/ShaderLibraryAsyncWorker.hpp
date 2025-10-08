#pragma once
#include "GfxDriver/ShaderProgram.hpp"
#include "Rendering/Shader.hpp"
#include "Shader2.hpp"
#include <memory>
#include <slang-com-ptr.h>
#include <slang.h>
#include <spdlog/spdlog.h>

#define MAX_SHADER_FEATURE_COUNT 64
using ShaderPermutation = std::bitset<MAX_SHADER_FEATURE_COUNT>;

struct ShaderToggleFeature
{
    std::string name = "";
    bool defaultValue = false;
};

struct ShaderFeatures
{
    template <class Iterable>
    ShaderPermutation GetPermutation(const Iterable& names) const
    {
        ShaderPermutation perm{};
        for (const std::string& name : names)
        {
            uint32_t bitIndex = 0;
            auto iter = featureToBitMask.find(name);
            if (iter != featureToBitMask.end())
            {
                bitIndex = iter->second;
                perm.set(bitIndex, true);
            }
        }

        return perm;
    }

    std::vector<std::string> GetFeautresFromBitmask(ShaderPermutation permutation) const
    {
        std::vector<std::string> result{};
        for (int bit = 0; bit < permutation.size(); ++bit)
        {
            if (permutation.test(bit))
            {
                result.push_back(bitMaskToFeature.at(bit));
            }
        }

        return result;
    }

    std::unordered_map<uint32_t, std::string> bitMaskToFeature{};
    std::unordered_map<std::string, uint32_t> featureToBitMask{};
    std::vector<ShaderToggleFeature> toggleFeatures{};
};

struct AsyncCompiledData
{
    AsyncCompiledData() noexcept = default;

    AsyncCompiledData(std::string&& name, ShaderPermutation&& permutation, ShaderFeatures&& shaderFeature, std::unique_ptr<Gfx::ShaderProgram>&& shader) noexcept
        : name(std::move(name)), permutation(std::move(permutation)), shaderFeature(std::move(shaderFeature)), shader(std::move(shader)) {}

    AsyncCompiledData(const std::string& name, const ShaderPermutation& permutation, const ShaderFeatures& shaderFeature, std::unique_ptr<Gfx::ShaderProgram>&& shader) noexcept
        : name(name), permutation(permutation), shaderFeature(shaderFeature), shader(std::move(shader)) {}

    // Copy constructor (deleted because of unique_ptr)
    AsyncCompiledData(const AsyncCompiledData& other) = delete;
    AsyncCompiledData& operator=(const AsyncCompiledData& other) = delete;

    // Move constructor and assignment
    AsyncCompiledData(AsyncCompiledData&& other) noexcept
        : name(std::move(other.name)), permutation(std::move(other.permutation)),
          shaderFeature(std::move(other.shaderFeature)), shader(std::move(other.shader)) {}

    AsyncCompiledData& operator=(AsyncCompiledData&& other) noexcept
    {
        if (this != &other) {
            name = std::move(other.name);
            permutation = std::move(other.permutation);
            shaderFeature = std::move(other.shaderFeature);
            shader = std::move(other.shader);
        }
        return *this;
    }

    ~AsyncCompiledData() noexcept = default;

    std::string name;
    ShaderPermutation permutation;
    ShaderFeatures shaderFeature;
    std::unique_ptr<Gfx::ShaderProgram> shader;
};

class ShaderLibraryAsyncWorker
{
public:
    ShaderLibraryAsyncWorker();
    ~ShaderLibraryAsyncWorker();
    void CompileShader(const char* name, ShaderPermutation permutation);
    void WaitForAll();
    void ReloadAllShaders();
    void CleanUp();
    std::optional<AsyncCompiledData> PollCompiled();

private:
    class CompileWorker;

    std::unique_ptr<CompileWorker> compileWorker;
};
