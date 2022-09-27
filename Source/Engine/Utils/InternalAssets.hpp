#pragma once
#include <filesystem>

namespace Engine::Utils
{
    class InternalAssets
    {
        public:
            static std::filesystem::path GetInternalAssetPath();
    };
}
