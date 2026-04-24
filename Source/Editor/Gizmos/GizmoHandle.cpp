#include "GizmoHandle.hpp"


std::list<GizmoState>& GizmoHandle::GetInvalidList()
{
	static std::list<GizmoState>invalidList = {};
	return invalidList;
}

GizmoHandle::GizmoHandle() { selfNode = GetInvalidList().end(); }

bool GizmoHandle::IsValid() const { return selfNode != GetInvalidList().end() && *isNodeValid; }