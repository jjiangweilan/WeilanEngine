import WeilanEngine;
import Library.PerScene;
import SceneLit; // use it's Vertex function
import Lighting; // use it's lighting function

[shader("vertex")]
VertexOutput VertexMain(VertexInput input, uint vertexID : SV_VertexID)
{
    return SceneLitVertex(input, vertexID);
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    return SceneLitFragment(input);
}
