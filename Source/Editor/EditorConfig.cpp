#include "EditorConfig.hpp"

EditorConfig& EditorConfig::GetInstance()
{
    static EditorConfig config;
    return config;
}
