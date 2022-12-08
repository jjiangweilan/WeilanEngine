#pragma once
#include <vector>
#include <list>
#include "AssetFile.hpp"

namespace Engine
{
    class DirectoryNode
    {
        public:

        private:

            std::list<DirectoryNode*> children;
            std::list<RefPtr<AssetFile>> files;
    };
}
