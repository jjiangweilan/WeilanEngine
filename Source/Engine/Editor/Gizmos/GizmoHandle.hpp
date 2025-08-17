#pragma once
#include <cstdint>

class GizmoHandle
{
public:
    bool Initialized() { return idx != 0; }

private:
    uint32_t idx;
    uint32_t generation;

    friend class GizmoContext;
};
