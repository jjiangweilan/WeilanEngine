#pragma once
#include "Libs/DynamicArray.hpp"

class FoliageSystem
{
public:
    void Render();

private:
    std::vector<FoliagePatch> foliagePatches;
};
