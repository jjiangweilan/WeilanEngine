import WeilanEngine;
import Library.PerScene;
import SceneLit; // use it's Vertex function
import Lighting; // use it's lighting function

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex, uint instanceID : SV_InstanceID)
{
    return SceneLitVertex(vertexID, drawIndex, instanceID);
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    return SceneLitFragment(input);
}
