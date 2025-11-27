#include "OceanQuadTree.hpp"
#include "Core/Math/Geometry.hpp"

void OceanQuadTree::UpdateQuadTree(const float3& center3f)
{
    nodePool.Clear();
    rootNodes.clear();

    auto& config = quadTreeConfig;
    float2 center = {center3f.x, center3f.z};

    // Generate max LOD first //
    int maxLod = config.mipLevels - 1;
    float2 min, max;
    min = {
        config.lodMaxPatchSize * glm::floor((center.x - config.lodViewDistance[maxLod]) / config.lodMaxPatchSize),
        config.lodMaxPatchSize * glm::floor((center.y - config.lodViewDistance[maxLod]) / config.lodMaxPatchSize)
    };
    max = {
        config.lodMaxPatchSize * glm::ceil((center.x + config.lodViewDistance[maxLod]) / config.lodMaxPatchSize),
        config.lodMaxPatchSize * glm::ceil((center.y + config.lodViewDistance[maxLod]) / config.lodMaxPatchSize)
    };

    for (int x = min.x; x < max.x; x += config.lodMaxPatchSize)
    {
        for (int y = min.y; y < max.y; y += config.lodMaxPatchSize)
        {
            auto node = nodePool.Allocate();
            InitNode(node, x, y, maxLod);
            rootNodes.push_back(node);
        }
    }

    for (auto& node : rootNodes)
    {
        DivideNode(node, maxLod, center);
    }
}

void OceanQuadTree::InitNode(QuadTreeNode node, int x, int y, int lodLevel)
{
    node->infoIndex = lodLevel;
    node->levelCoord = {x / quadTreeConfig.lodMaxPatchSize, y / quadTreeConfig.lodMaxPatchSize};
    node->minPos = {x, y};
    node->maxPos = {x + quadTreeConfig.lodMaxPatchSize, y + quadTreeConfig.lodMaxPatchSize};
    node->children = {};
}

void OceanQuadTree::DivideNode(QuadTreeNode node, int lodLevel, const float2& center)
{
    if (lodLevel == 0)
    {
        return;
    }

    Circle circle{center, quadTreeConfig.lodViewDistance[lodLevel - 1]};
    Quad2D quad{node->minPos, node->maxPos};
    if (CircleVsQuad2D(circle, quad))
    {
        for (int i = 0; i < 4; ++i)
        {
            auto node = nodePool.Allocate();
        }
    }
}
