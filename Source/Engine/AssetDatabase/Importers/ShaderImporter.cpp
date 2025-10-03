#include "ShaderImporter.hpp"
#include "Rendering/Shader.hpp"

DEFINE_ASSET_IMPORTER(ShaderImporter, "shad,comp")

const std::vector<std::type_index>& ShaderImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Obsolete::Shader), typeid(Obsolete::ComputeShader)};
    return types;
}
