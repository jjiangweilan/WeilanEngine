#pragma once
#include "../NodeBlueprint.hpp"

namespace FrameGraph
{
class TerrainNode : public Node
{
    DECLARE_OBJECT();

public:
    TerrainNode();
    TerrainNode(FGID id);

    static char _reg;
};
} // namespace FrameGraph

#pragma once
