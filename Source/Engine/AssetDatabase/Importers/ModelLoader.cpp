#include "ModelLoader.hpp"
#include "Core/Model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
DEFINE_ASSET_LOADER(ModelLoader, "glb")

struct ImporterImple
{
    void Load(const std::filesystem::path& path)
    {
        Assimp::Importer importer;
        auto scene = importer.ReadFile(path.string().c_str(), aiProcess_JoinIdenticalVertices | aiProcess_Triangulate);

        if (scene == nullptr)
        {
            spdlog::error(importer.GetErrorString());
            return;
        }

        ProcessNode(scene, scene->mRootNode);
    }

    void ProcessNode(const aiScene* scene, aiNode* node)
    {
        for(int meshIndex = 0; meshIndex < node->mNumMeshes; meshIndex++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        }
    }
};

const std::vector<std::type_index>& ModelLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Model)};
    return types;
}

void ModelLoader::Load()
{
    auto ext = absoluteAssetPath.extension();
    asset = std::make_unique<Model>();
    asset->LoadFromFile(absoluteAssetPath.string().c_str());
}
