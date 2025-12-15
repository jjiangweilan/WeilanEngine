#pragma once
#include "Library/Math/Geometry/Geometry.hpp"
#include "Library/Math.hpp"
#include "Library/ObjectPool.hpp"
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
    float resolution; // 1 : 1 to node
    int mipLevels;
    std::span<float> lodViewDistance; // size of mipLevels
};

struct OceanQuadTreeConfigExtended
{
    float nodeResolution;
    int lodMinNodeSize; // in powers of two
    int lodMaxNodeSize; // in powers of two
    int mipLevels;
    std::vector<float> lodViewDistance; // size of mipLevels
};

class OceanQuadTree
{
public:
    void SetLODLevels(const OceanQuadTreeConfig& config);
    void UpdateQuadTree(const float3& center, const Frustum& cameraFrustum);

    std::vector<QuadTreeNode>& GetNodesAtLOD(int lodLevel);

    const OceanQuadTreeConfigExtended& GetQuadTreeInfo() { return quadTreeConfig; }

private:
    struct NodeLodInfo
    {
        int level;
        int nodeSize;
    };

    using LodLevelNodes = std::vector<QuadTreeNode>;

    const int nodeMeshMeters = 32;

    void DivideNode(QuadTreeNode node, const float2& center, const Frustum& cameraFrustum);
    void InitNode(QuadTreeNode node, int x, int y, int lodLevel);

    std::vector<LodLevelNodes> nodeToRender;
    std::vector<QuadTreeNode> rootNodes;
    ObjectPool<QuadTreeNode_t> nodePool;
    OceanQuadTreeConfigExtended quadTreeConfig;
    std::vector<NodeLodInfo> lodInfos;
};
