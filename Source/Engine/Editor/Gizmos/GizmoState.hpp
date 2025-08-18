#pragma once
#include "./GizmoBase.hpp"

struct GizmoState
{
    bool isHot = false;
    bool active = false;
    std::unique_ptr<GizmoBase> ptr = nullptr;
};

using GizmoList = std::list<GizmoState>;
