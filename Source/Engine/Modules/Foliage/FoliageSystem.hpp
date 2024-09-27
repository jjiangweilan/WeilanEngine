#pragma once
#include <vector>

class FoliageSystem
{
public:
    void Render();

private:
    std::vector<FoliagePatch> foliagePatches;
};
