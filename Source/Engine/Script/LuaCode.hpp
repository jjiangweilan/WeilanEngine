#pragma once
#include "Core/AssetObject.hpp"

namespace Engine
{
    class LuaCode : public AssetObject
    {

        public:
            LuaCode(const UUID& uuid = UUID::empty);
    };
}
