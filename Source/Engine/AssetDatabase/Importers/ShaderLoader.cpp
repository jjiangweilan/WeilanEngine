#include "ShaderLoader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include <spirv_cross/spirv_reflect.hpp>

DEFINE_ASSET_LOADER(ShaderLoader, "shad,comp")

const DynamicArray<std::type_index>& ShaderLoader::GetImportTypes()
{
    static DynamicArray<std::type_index> types = {typeid(Obsolete::Shader), typeid(Obsolete::ComputeShader)};
    return types;
}

bool ShaderLoader::ImportNeeded()
{
    return false;
}

void ShaderLoader::Load()
{
    nlohmann::json shaderPasses = meta["compiledShaderPasses"];
    DynamicArray<std::unique_ptr<Obsolete::ShaderPass>> passes;

    std::unique_ptr<Obsolete::ShaderBase> shader;
    if (absoluteAssetPath.extension() == ".shad")
    {
        shader = std::make_unique<Obsolete::Shader>();
    }
    else if (absoluteAssetPath.extension() == ".comp")
    {
        shader = std::make_unique<Obsolete::ComputeShader>();
    }

    shader->SetShaderPasses(std::move(passes));
    shader->SetName(absoluteAssetPath.filename().string());
    asset = std::move(shader);
    //
    // shader->LoadFromFile(absoluteAssetPath.string().c_str());
}
