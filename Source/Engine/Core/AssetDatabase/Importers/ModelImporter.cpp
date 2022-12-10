#include "ModelImporter.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Engine::Internal
{
    static void ProcessNode(aiScene* scene, aiNode *node, aiMatrix4x4 chainTransform);
    static void LoadMesh(aiScene* scene, aiMesh *mesh, aiNode *node, const aiMatrix4x4& chainTransform);

    void ModelImporter::Import(
            const std::filesystem::path& path,
            const std::filesystem::path& root,
            const nlohmann::json& json,
            const UUID& rootUUID,
            const std::unordered_map<std::string, UUID>& containedUUIDs)
    {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(path.string(),
                aiProcess_CalcTangentSpace       |
                aiProcess_Triangulate            |
                aiProcess_JoinIdenticalVertices  |
                aiProcess_SortByPType);

        if (!scene)
        {
            return;
        }

        return;
    }


    UniPtr<AssetObject> ModelImporter::Load(
            const std::filesystem::path& root,
            ReferenceResolver& refResolver,
            const UUID& uuid)
    {
        return nullptr;
    }

    bool ModelImporter::NeedReimport(
            const std::filesystem::path& path,
            const std::filesystem::path& root,
            const UUID& uuid)
    {
        return false;
    }

    void ProcessNode(aiScene* scene, aiNode *node, aiMatrix4x4 chainTransform)
    {
        auto transform =  chainTransform * node->mTransformation;// assimp is row major
        for (size_t i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            LoadMesh(scene, mesh, node, transform);
        }
        for (size_t i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(scene, node->mChildren[i], transform);
        }
    }

    void LoadMesh(aiScene* scene, aiMesh *mesh, aiNode *node, const aiMatrix4x4& chainTransform)
    {

    }
};
