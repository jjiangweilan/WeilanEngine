#pragma once
#include "Core/Component/Component.hpp"

class ImageUI : public Component
{
    DECLARE_OBJECT();

public:
private:
    float2 pivot = {0, 0};
    float depth = 0;
};
