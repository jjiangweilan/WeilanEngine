#include "LegacyLoader.hpp"
#include "Core/Model.hpp"
#include "Rendering/Shader.hpp"


DEFINE_ASSET_LOADER(LegacyLoader, "shad,comp,glb")

void LegacyLoader::Load()
{
    auto ext = absoluteAssetPath.extension();
    if (ext == ".shad")
        asset = std::make_unique<Shader>();
    else if (ext == ".comp")
        asset = std::make_unique<ComputeShader>();
    else if (ext == ".glb")
        asset = std::make_unique<Model>();
    asset->LoadFromFile(absoluteAssetPath.string().c_str());
}
