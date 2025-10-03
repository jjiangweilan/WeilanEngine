#include "ModelImporter.hpp"
#include "Core/Model.hpp"

DEFINE_ASSET_IMPORTER(ModelImporter, "glb,gltf,fbx");

const std::vector<std::type_index>& ModelImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}
