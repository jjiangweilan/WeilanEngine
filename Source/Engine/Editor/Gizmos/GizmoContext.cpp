#include "GizmoContext.hpp"

void GizmoContext::DrawScaleBox(GizmoHandle& handle, const float3& position, float3& inoutSize)
{
    if (!IsHandleCreated(handle))
    {
        GetHandleID(handle.id, handle.generation);
    }
}

void GizmoContext::ClearInactiveGizmos()
{
    const int MAX_REMOVE_PER_CALL = 8;
    int inactiveIndices[MAX_REMOVE_PER_CALL]{};
    int removeIdx = 0;
    int currIdx = 0;
    for (auto& g : allGizmos)
    {
        if (removeIdx >= MAX_REMOVE_PER_CALL)
            break;

        if (!g->m_IsActive)
        {
            inactiveIndices[removeIdx++] = currIdx;
        }

        currIdx += 1;
    }

    for (int i = 0; i < removeIdx; i++)
    {
        std::swap(allGizmos[inactiveIndices[i]], allGizmos.back());
        allGizmos.pop_back();
    }
}

GizmoState GizmoContext::GetGizmoState(GizmoHandle& handle)
{
    if (!handle.Initialized())
    {}
}

void GizmoContext::GetHandleID(uint32_t& outID, uint32_t& outGeneration) {

}
