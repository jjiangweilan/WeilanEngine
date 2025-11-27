#pragma once
#include "Libs/Math.hpp"
#include "Libs/ObjectPool.hpp"

struct QuadTreeNode_t;
using QuadTreeNode = ObjectPoolHandle<QuadTreeNode_t>;

struct QuadTreeNode_t
{
    std::vector<QuadTreeNode> children;
};

struct OceanQuadTreeLOD
{
};

class OceanQuadTree
{
public:
    void SetLODLevels();
    void UpdateQuadTree(const float3& center);

private:
    std::vector<QuadTreeNode> rootNodes;
    ObjectPool<QuadTreeNode> nodePool;
};
