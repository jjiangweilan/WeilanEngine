#pragma once
#include "Libs/UUID.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>

namespace Engine
{
    class AssetImportCache
    {
        public:

            struct CacheData
            {
                nlohmann::json importConfig;
                unsigned char* data;
            };

            AssetImportCache(const std::filesystem::path& root) : root(root) {};
            CacheData GetCache(const UUID& uuid);
            void WriteCacheData(const UUID& uuid, CacheData cacheData);
        private:
            const std::filesystem::path root;
            nlohmann::json importInfo;
    };
}
