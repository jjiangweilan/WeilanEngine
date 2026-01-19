#include "OceanQuadTree.hpp"
#include "Engine/Library/Math/Geometry/Geometry.hpp"

void OceanQuadTree::UpdateQuadTree(const float3& center3f, const Frustum& cameraFrustum)
{
    nodePool.Clear();
    rootNodes.clear();
    for (auto& lodNodes : nodeToRender)
    {
        lodNodes.clear();
    }

    auto& config = quadTreeConfig;
    float2 center = {center3f.x, center3f.z};

    // Generate max LOD first //
    int maxLod = config.mipLevels - 1;
    float2 min, max;
    min = {
        config.lodMaxNodeSize * glm::floor((center.x - config.lodViewDistance[maxLod]) / config.lodMaxNodeSize),
        config.lodMaxNodeSize * glm::floor((center.y - config.lodViewDistance[maxLod]) / config.lodMaxNodeSize)
    };
    max = {
        config.lodMaxNodeSize * glm::ceil((center.x + config.lodViewDistance[maxLod]) / config.lodMaxNodeSize),
        config.lodMaxNodeSize * glm::ceil((center.y + config.lodViewDistance[maxLod]) / config.lodMaxNodeSize)
    };

    for (int x = min.x; x < max.x; x += config.lodMaxNodeSize)
    {
        for (int y = min.y; y < max.y; y += config.lodMaxNodeSize)
        {
            auto node = nodePool.AllocateHandle();
            InitNode(node, x, y, maxLod);

            AABB aabb(float3(node->minPos.x, -100, node->minPos.y), float3(node->maxPos.x, 100, node->maxPos.y));
            if (AABBVsFrustum(aabb, cameraFrustum))
            {
                rootNodes.push_back(node);
            }
            else
            {
                nodePool.Free(node);
            }
        }
    }

    for (auto& node : rootNodes)
    {
        DivideNode(node, center, cameraFrustum);
    }
}

void OceanQuadTree::InitNode(QuadTreeNode node, int x, int y, int lodLevel)
{
    const auto& lodInfo = lodInfos[lodLevel];
    node->lodLevel = lodLevel;
    node->levelCoord = {x / lodInfo.nodeSize, y / lodInfo.nodeSize};
    node->minPos = {x, y};
    node->maxPos = {x + lodInfo.nodeSize, y + lodInfo.nodeSize};
    node->children.clear();
}

void OceanQuadTree::DivideNode(QuadTreeNode node, const float2& center, const Frustum& cameraFrustum)
{
    // this function is called when this node doesn't need to be divided
    auto pushToLodRendering = [this](QuadTreeNode& node)
    {
        this->nodeToRender[node->lodLevel].push_back(node);
    };

    if (node->lodLevel == 0)
    {
        pushToLodRendering(node);
        return;
    }

    int nextLodLevel = node->lodLevel - 1;
    Circle circle{center, quadTreeConfig.lodViewDistance[nextLodLevel]};
    Quad2D quad{node->minPos, node->maxPos};
    if (CircleVsQuad2D(circle, quad))
    {
        int nextNodeSize = lodInfos[nextLodLevel].nodeSize;
        for (int i = 0; i < 4; ++i)
        {
            int lx = i & 0b01 ? 1 : 0;
            int ly = i & 0b10 ? 1 : 0;
            auto child = nodePool.AllocateHandle();
            InitNode(child, node->minPos.x + lx * nextNodeSize, node->minPos.y + ly * nextNodeSize, nextLodLevel);

            AABB aabb(float3(child->minPos.x, -100, child->minPos.y), float3(child->maxPos.x, 100, child->maxPos.y));
            if (AABBVsFrustum(aabb, cameraFrustum))
            {
                node->children.push_back(child);
            }
            else
            {
                nodePool.Free(child);
            }
        }

        for (auto& c : node->children)
        {
            DivideNode(c, center, cameraFrustum);
        }
    }
    else
    {
        pushToLodRendering(node);
    }
}

void OceanQuadTree::SetLODLevels(const OceanQuadTreeConfig& config)
{
    const int lodMinNodeSize = config.resolution * nodeMeshMeters;
    this->quadTreeConfig.nodeResolution = config.resolution;
    this->quadTreeConfig.lodMinNodeSize = lodMinNodeSize;
    this->quadTreeConfig.lodMaxNodeSize = lodMinNodeSize << (config.mipLevels - 1);
    this->quadTreeConfig.mipLevels = config.mipLevels;
    this->quadTreeConfig.lodViewDistance = std::vector<float>(config.lodViewDistance.begin(), config.lodViewDistance.end());

    lodInfos.clear();
    nodeToRender.clear();
    for (int lodLevel = 0; lodLevel < config.mipLevels; ++lodLevel)
    {
        NodeLodInfo info;
        info.level = lodLevel;
        info.nodeSize = quadTreeConfig.lodMinNodeSize << lodLevel;
        lodInfos.push_back(info);

        nodeToRender.push_back({});
    }
}

std::vector<QuadTreeNode>& OceanQuadTree::GetNodesAtLOD(int lodLevel)
{
    return nodeToRender[lodLevel];
}
