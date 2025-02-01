# 000 Validation for CommandBuffer Submittion
Validation for command buffer should be done as early as possible.
The first validation should happen in `CommandBuffer`, then `RenderGraph`.

# 001 Write Gizmo
Gizmos is an immediate mode API that builds up the relavent data each frame. You should subclass GizmoBase for actual implementation and provie a static member in `Gizmos` class as its public API
