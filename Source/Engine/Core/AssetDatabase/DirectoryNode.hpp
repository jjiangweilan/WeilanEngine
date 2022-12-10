#pragma once
#include <vector>
#include <list>
#include "AssetFile.hpp"

namespace Engine
{
    struct DirectoryNode
    {
        std::list<DirectoryNode*> children;
        std::list<RefPtr<AssetFile>> files;
    };
}
