#pragma once
#include "Libs/UUID.hpp"
#include <boost/unordered/concurrent_flat_map.hpp>

enum class AssetLoadingStatus
{
    Ready,
    Loading,
    NotLoaded
};

class AsyncAssetLoader
{
public:

private:
    boost::unordered::concurrent_flat_map<UUID, AssetLoadingStatus> assetStatusMap;
};
