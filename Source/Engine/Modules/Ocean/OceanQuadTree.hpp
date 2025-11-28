#pragma once
#include "Libs/Math.hpp"
#include "Libs/ObjectPool.hpp"
#include <span>

struct QuadTreeNode_t;
using QuadTreeNode = ObjectPoolHandle<QuadTreeNode_t>;

struct QuadTreeNode_t
{
    int lodLevel;
    int2 levelCoord;
    int2 minPos;
    int2 maxPos;
    std::vector<QuadTreeNode> children;
};

struct OceanQuadTreeConfig
{
    int lodMinPatchSize; // in powers of two
    int mipLevels;
    std::span<float> lodViewDistance; // size of mipLevels
};

class OceanQuadTree
{
public:
    void SetLODLevels(const OceanQuadTreeConfig& config);
    void UpdateQuadTree(const float3& center);

private:
    struct NodeLodInfo
    {
        int level;
        int patchSize;
    };

    struct OceanQuadTreeConfigExtended
    {
        int lodMinPatchSize; // in powers of two
        int lodMaxPatchSize; // in powers of two
        int mipLevels;
        std::vector<float> lodViewDistance; // size of mipLevels
    };

    void DivideNode(QuadTreeNode node, const float2& center);
    void InitNode(QuadTreeNode node, int x, int y, int lodLevel);

    std::vector<QuadTreeNode> rootNodes;
    ObjectPool<QuadTreeNode_t> nodePool;
    OceanQuadTreeConfigExtended quadTreeConfig;
    std::vector<NodeLodInfo> lodInfos;
};
