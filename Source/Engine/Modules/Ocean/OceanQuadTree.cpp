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
        DivideNode(node, center);
    }
}

void OceanQuadTree::InitNode(QuadTreeNode node, int x, int y, int lodLevel)
{
    const auto& lodInfo = lodInfos[lodLevel];
    node->lodLevel = lodLevel;
    node->levelCoord = {x / lodInfo.patchSize, y / lodInfo.patchSize};
    node->minPos = {x, y};
    node->maxPos = {x + lodInfo.patchSize, y + lodInfo.patchSize};
    node->children.clear();
}

void OceanQuadTree::DivideNode(QuadTreeNode node, const float2& center)
{
    if (node->lodLevel == 0)
    {
        return;
    }

    int nextLodLevel = node->lodLevel - 1;
    Circle circle{center, quadTreeConfig.lodViewDistance[nextLodLevel]};
    Quad2D quad{node->minPos, node->maxPos};
    if (CircleVsQuad2D(circle, quad))
    {
        int nextPatchSize = lodInfos[nextLodLevel].patchSize;
        for (int i = 0; i < 4; ++i)
        {
            int lx = i & 0b01 ? 1 : 0;
            int ly = i & 0b10 ? 1 : 0;
            auto child = nodePool.Allocate();
            InitNode(child, node->minPos.x + lx * nextPatchSize, node->minPos.y + ly * nextPatchSize, nextLodLevel);
            node->children.push_back(child);
        }
    }
}

void OceanQuadTree::SetLODLevels(const OceanQuadTreeConfig& config)
{
    this->quadTreeConfig.lodMaxPatchSize = config.lodMinPatchSize << (config.mipLevels - 1);
    this->quadTreeConfig.lodMinPatchSize = config.lodMinPatchSize;
    this->quadTreeConfig.mipLevels = config.mipLevels;
    this->quadTreeConfig.lodViewDistance = std::vector<float>(config.lodViewDistance.begin(), config.lodViewDistance.end());

    lodInfos.clear();
    for (int lodLevel = 0; lodLevel < config.mipLevels; ++lodLevel)
    {
        NodeLodInfo info;
        info.level = lodLevel;
        info.patchSize = config.lodMinPatchSize << lodLevel;
        lodInfos.push_back(info);
    }
}
