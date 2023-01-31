#pragma once
#include "AssetFile.hpp"
#include <list>
#include <vector>

namespace Engine
{
struct DirectoryNode
{
    std::list<DirectoryNode*> children;
    std::list<RefPtr<AssetFile>> files;
};
} // namespace Engine
