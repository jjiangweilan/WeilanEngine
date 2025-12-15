#include "GeometryRendering.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"

void GeometryRendering::DrawWireBox(Gfx::CommandBuffer& cmd, const Box& box)
{
    int lineaIndices[] = {
        0, 1, 1, 3, 3, 2, 2, 0, // bottom
        4, 5, 5, 7, 7, 6, 6, 4, // top
        0, 4, 1, 5, 2, 6, 3, 7  // sides
    };

    // 12 edges -> 24 indices (pairs)
    for (int i = 0; i < 12; i++)
    {
        int fromIndex = lineaIndices[2 * i + 0];
        int toIndex   = lineaIndices[2 * i + 1];

        struct
        {
            float4 fromPos, toPos;
            float4 color;
        } data;
        data.fromPos = float4(box.points[fromIndex], 1.0f);
        data.toPos = float4(box.points[toIndex], 1.0f);
        data.color = float4(1, 1, 1, 1);

        Gfx::ShaderProgram* lineShaderProgram = EngineInternalResources::GetLineShader().GetShaderProgram();
        cmd.SetPushConstant(lineShaderProgram, (void*)&data);
        cmd.BindShaderProgram(lineShaderProgram, lineShaderProgram->GetDefaultShaderConfig());
        cmd.Draw(2, 1, 0, 0);
    }
}
