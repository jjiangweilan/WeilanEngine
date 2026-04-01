#pragma once
#include <string>
#include <vector>
#include <optional>

namespace Editor
{
    struct Range
    {
        float min = 0.0f;
        float max = 1.0f;
    };

    struct Group
    {
        std::string name;
    };

    struct Tooltip
    {
        std::string text;
    };

    struct MaterialAttributeInfo
    {
        std::optional<Range> range;
        bool isColor = false;
        std::optional<Group> group;
        std::optional<Tooltip> tooltip;
    };

    MaterialAttributeInfo ParseAttributes(const std::vector<std::string>& rawAttributes);
}
