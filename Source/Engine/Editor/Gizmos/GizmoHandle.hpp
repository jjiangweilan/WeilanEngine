#pragma once
#include "./GizmoState.hpp"
#include <cstdint>
#include <list>

class GizmoHandle
{
public:
    GizmoHandle() { selfNode = invalidList.end(); }
    bool IsValid() const { return selfNode != invalidList.end(); }
    bool IsActive() const
    {
        if (IsValid())
            return selfNode->active;
        else
            return false;
    }

private:
    static std::list<GizmoState> invalidList;
    std::list<GizmoState>::iterator selfNode;

    friend class GizmoContext;
};
