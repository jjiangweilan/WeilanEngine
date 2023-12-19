#pragma once
#include "Core/AssetObject.hpp"

class LuaCode : public AssetObject
{

public:
    LuaCode(const UUID& uuid = UUID::empty);
};
