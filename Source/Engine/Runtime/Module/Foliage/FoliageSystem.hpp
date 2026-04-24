#pragma once
#include "Engine/Library/DynamicArray.hpp"

class FoliageSystem
{
public:
    void Render();

private:
    std::vector<FoliagePatch> foliagePatches;
};
