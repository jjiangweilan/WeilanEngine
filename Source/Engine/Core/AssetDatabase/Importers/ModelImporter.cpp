#include "ModelImporter.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Engine::Internal
{
    UniPtr<AssetObject> ModelImporter::Import(
        const std::filesystem::path& path,
        RefPtr<AssetImportCache> importCache,
        const nlohmann::json& config, ReferenceResolver& refResolver, const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs)
    {
        auto cached = ReadCache(uuid);

        if (cached != nullptr) return cached;

        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(path.string(), 
                aiProcess_CalcTangentSpace       | 
                aiProcess_Triangulate            |
                aiProcess_JoinIdenticalVertices  |
                aiProcess_SortByPType);
        
        if (!scene)
        {
            return nullptr;
        }

        return nullptr;
    }

    UniPtr<AssetObject> ModelImporter::ReadCache(const UUID& uuid)
    {
        return nullptr;
    }
};
