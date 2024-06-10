#pragma once
#include <functional>

namespace Libs::Image
{
/* level 0
 -node(id:0)-node(id:1)-node(id:2)-node(id:3)-
 -node(id:4)-node(id:5)-node(id:6)-node(id:7)-
 -node(id:8)-node(id:9)-node(id:10)-node(id:11)-
 */

/* level 1
 -node(id:12)-node(id:13)-
 -node(id:14)-node(id:15)-
 */

/* level 2

 -node(id:16)-

 */
template <class T>
class MipQuadTree
{
public:
    void Resize(int extentX, int extentY)
    {
        this->extentX = extentX;
        this->extentY = extentY;
        assert(extentX % 2 == 0);
        assert(extentY % 2 == 0);
        int level = 0;
        int nodeCount = 0;
        maxLevel = extentX < extentY ? std::log2(extentX) : std::log2(extentY);
        while (level <= maxLevel)
        {
            int mipExtentX = extentX / std::pow(2, level);
            int mipExtentY = extentY / std::pow(2, level);
            nodeCount += mipExtentX * mipExtentY;
            level += 1;
        }
        nodes.resize(nodeCount);

        int index = 0;
        level = 0;
        while (level <= maxLevel)
        {
            int mipExtentX = extentX / std::pow(2, level);
            int mipExtentY = extentY / std::pow(2, level);
            for (int h = 0; h < mipExtentY; ++h)
            {
                for (int w = 0; w < mipExtentX; ++w)
                {
                    Node& currNode = nodes[index];
                    currNode.id = index;
                    currNode.x = w;
                    currNode.y = h;
                    currNode.level = level;
                    if (level < maxLevel)
                    {
                        int higherW = w / 2;
                        int higherH = h / 2;
                        int parentNode = GetNodeID(higherW, higherH, level + 1);
                        Node* parent = &nodes[parentNode];
                        currNode.parent = parent;

                        bool left = w % 2 == 0;
                        bool top = h % 2 == 0;
                        if (left && top)
                            parent->c00 = &currNode;
                        else if (left && !top)
                            parent->c01 = &currNode;
                        else if (!left && top)
                            parent->c10 = &currNode;
                        else if (!left && !top)
                            parent->c11 = &currNode;
                    }

                    index += 1;
                }
            }

            level += 1;
        }
    }
    using NodeID = int;
    struct Node
    {
        NodeID id = -1;
        Node* c00 = nullptr;
        Node* c01 = nullptr;
        Node* c10 = nullptr;
        Node* c11 = nullptr;
        Node* parent = nullptr;

        int x = 0;
        int y = 0;
        int level = -1;
        T val;
    };

    Node* GetNode(NodeID id)
    {
        if (id < nodes.size())
        {
            return &nodes[id];
        }
        return nullptr;
    }
    std::vector<Node>& GetNodes() { return nodes; }
    int GetMaxLevel() { return maxLevel; }

private:
    NodeID GetNodeID(int x, int y, int level)
    {
        int l = 0;

        int offset = 0;
        int mipExtentX;
        int mipExtentY;
        while (l < level)
        {
            mipExtentX = extentX / std::pow(2, l);
            mipExtentY = extentY / std::pow(2, l);

            offset += mipExtentX * mipExtentY;
            l += 1;
        }

        mipExtentX = extentX / std::pow(2, l);

        return x + y * mipExtentX + offset;
    }

    std::vector<Node> nodes;
    int extentX;
    int extentY;
    int maxLevel;
};

template <class NodeType>
void WalkUpMipQuadTree(NodeType& node, std::function<bool(NodeType&)> Visit)
{
    bool stop = Visit(node);
    if (node.parent && !stop)
    {
        WalkUpMipQuadTree(*node.parent, Visit);
    }
};
} // namespace Libs::Image
