#pragma once
#include "./GizmoBase.hpp"
#include <list>
struct GizmoState
{
    bool isHot = false;
    bool active = false;
    std::unique_ptr<GizmoBase> ptr = nullptr;
    std::shared_ptr<bool> isHandleValid;

    // Delete copy constructor and copy assignment
    GizmoState(const GizmoState&) = delete;
    GizmoState& operator=(const GizmoState&) = delete;

    // Define move constructor and move assignment
    GizmoState(GizmoState&&) = default;
    GizmoState& operator=(GizmoState&&) = default;

    // Default constructor
    GizmoState() = default;
};

using GizmoList = std::list<GizmoState>;
