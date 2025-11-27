#pragma once
#include "Libs/Math.hpp"
#include "Libs/ObjectPool.hpp"
#include <span>

struct QuadTreeNode_t;
using QuadTreeNode = ObjectPoolHandle<QuadTreeNode_t>;

struct QuadTreeNode_t
{
    int infoIndex;
    int2 levelCoord;
    int2 minPos;
    int2 maxPos;
    std::vector<QuadTreeNode> children;
};

struct OceanQuadTreeConfig
{
    int lodMaxPatchSize; // in powers of two
    int mipLevels;
    std::span<float> lodViewDistance; // size of mipLevels
};

class OceanQuadTree
{
public:
    void SetLODLevels(const OceanQuadTreeConfig& config) { this->quadTreeConfig = config; }
    void UpdateQuadTree(const float3& center);

private:
    struct NodeInfo
    {
        int level;
        int size;
    };

    void DivideNode(QuadTreeNode node, int lodLevel, const float2& center);
    void InitNode(QuadTreeNode node, int x, int y, int lodLevel);

    const NodeInfo& Access(const QuadTreeNode& node) { return prototypes[node->infoIndex]; }
    bool SphereVsQuad();

    std::vector<QuadTreeNode> rootNodes;
    ObjectPool<QuadTreeNode_t> nodePool;
    OceanQuadTreeConfig quadTreeConfig;
    std::vector<NodeInfo> prototypes;
};
