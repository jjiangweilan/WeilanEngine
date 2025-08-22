#include "GizmoBase.hpp"

bool& GizmoBase::s_GizmoIsInteracting()
{
    static bool v = false;
    return v;
}
