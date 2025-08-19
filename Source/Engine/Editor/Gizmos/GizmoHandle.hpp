#pragma once
#include "./GizmoState.hpp"
#include <cstdint>
#include <list>

class GizmoHandle
{
public:
    GizmoHandle();
    bool IsValid() const;
    bool IsActive() const
    {
        if (IsValid())
            return selfNode->active;
        else
            return false;
    }


private:
    static std::list<GizmoState>& GetInvalidList();
    std::list<GizmoState>::iterator selfNode;
    std::shared_ptr<bool> isNodeValid = std::make_shared<bool>(false);

    friend class GizmoManager;
};
