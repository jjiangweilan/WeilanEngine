#pragma once
#include "../NodeBlueprint.hpp"

namespace Rendering::FrameGraph
{
class TerrainNode : public Node
{
    DECLARE_OBJECT();

public:
    TerrainNode(){};
    TerrainNode(FGID id) : Node("Terrain", id){};

    static char _reg;
};
} // namespace Rendering::FrameGraph

#pragma once
