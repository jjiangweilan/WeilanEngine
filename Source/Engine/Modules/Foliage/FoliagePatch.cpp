#include "FoliagePatch.hpp"

DEFINE_OBJECT(FoliagePatch, "96C9856F-1F2C-4F25-AF8B-ABD231403FE4");

std::unique_ptr<Component> FoliagePatch::Clone(GameObject& owner)
{
    return nullptr;
}

const std::string& FoliagePatch::GetName()
{
    static std::string s("Foliage");
    return s;
}
