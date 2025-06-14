#pragma once
#include "Core/Component/UI.hpp"

class DrawUI
{
public:
    DrawUI();

    void Draw(UI* ui);

private:
    const uint32_t perFrameBufferSize = 8 * 1024 * 1024; // 64MB per frame
};
