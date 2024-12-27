#include "ShaderLoader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include <spirv_cross/spirv_reflect.hpp>

DEFINE_ASSET_LOADER(ShaderLoader, "shad,comp")

const std::vector<std::type_index>& ShaderLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Shader), typeid(ComputeShader)};
    return types;
}

bool ShaderLoader::ImportNeeded()
{
    return false;
}

void ShaderLoader::Load()
{
    nlohmann::json shaderPasses = meta["compiledShaderPasses"];
    std::vector<std::unique_ptr<ShaderPass>> passes;

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
    shader->SetName(absoluteAssetPath.filename().string());
    asset = std::move(shader);
    //
    // shader->LoadFromFile(absoluteAssetPath.string().c_str());
}
