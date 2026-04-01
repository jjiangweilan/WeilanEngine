#include "MaterialAttributeParser.hpp"
#include "Engine/Library/Utils.hpp"
#include <algorithm>
#include <regex>

namespace Editor
{
    MaterialAttributeInfo ParseAttributes(const std::vector<std::string>& rawAttributes)
    {
        MaterialAttributeInfo info;

        for (const auto& attr : rawAttributes)
        {
            std::string lowerAttr = Utils::strToLower(attr);

            if (lowerAttr == "color")
            {
                info.isColor = true;
                continue;
            }

            // Regex for Range(min, max)
            static std::regex rangeRegex(R"(range\s*\(\s*([+-]?([0-9]*[.])?[0-9]+)\s*,\s*([+-]?([0-9]*[.])?[0-9]+)\s*\))", std::regex_constants::icase);
            std::smatch rangeMatch;
            if (std::regex_search(attr, rangeMatch, rangeRegex))
            {
                Range r;
                r.min = std::stof(rangeMatch[1].str());
                r.max = std::stof(rangeMatch[3].str());
                info.range = r;
                continue;
            }
            else if (lowerAttr == "range")
            {
                // Default range if no arguments
                info.range = Range{ 0.0f, 1.0f };
                continue;
            }

            // Regex for Group("name")
            static std::regex groupRegex(R"(group\s*\(\s*\"(.*)\"\s*\))", std::regex_constants::icase);
            std::smatch groupMatch;
            if (std::regex_search(attr, groupMatch, groupRegex))
            {
                info.group = Group{ groupMatch[1].str() };
                continue;
            }

            // Regex for Tooltip("text")
            static std::regex tooltipRegex(R"(tooltip\s*\(\s*\"(.*)\"\s*\))", std::regex_constants::icase);
            std::smatch tooltipMatch;
            if (std::regex_search(attr, tooltipMatch, tooltipRegex))
            {
                info.tooltip = Tooltip{ tooltipMatch[1].str() };
                continue;
            }
        }

        return info;
    }
}
