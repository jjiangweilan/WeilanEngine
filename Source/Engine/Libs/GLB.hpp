#pragma once
#include "Core/Graphics/Mesh.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include "Libs/DynamicArray.hpp"

class GameObject;
namespace Utils
{
class GLB
{
public:
    static void GetGLBData(
        const std::filesystem::path& path,
        DynamicArray<uint32_t>& fullData,
        nlohmann::json& jsonData,
        unsigned char*& binaryData
    );
    static void SetAssetName(Asset* asset, nlohmann::json& j, const std::string& assetGroupName, int index);
    static void SetGameObjectName(GameObject* asset, nlohmann::json& j, const std::string& assetGroupName, int index);
    static DynamicArray<std::unique_ptr<Mesh>> ExtractMeshes(
        nlohmann::json& jsonData, unsigned char*& binaryData, int maximumMesh = std::numeric_limits<int>::max()
    );
};
} // namespace Utils
