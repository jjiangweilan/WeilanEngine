#pragma once
#include "Libs/DynamicArray.hpp"

class FoliageSystem
{
public:
    void Render();

private:
    DynamicArray<FoliagePatch> foliagePatches;
};
