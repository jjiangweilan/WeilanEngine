#pragma once
#include <span>
#include <string>
#include <vector>

namespace Engine::FrameGraph
{

// frame graph ID, nodes, properties and links share the same id space
//(16 bit src unique node, 16 bit src property unique id), (16 bit node unique id), (16 bit property unique id)
// when it's a link id it uses all the 64 bit, the upper 32 bits and lower 32 bits are the target nodes and their
// note: the 32 bit all togethe makes a property id for editor
// properties respectively
using FGID = uint64_t;

// reverse 999 properties for each node, should be very enough
#define FRAME_GRAPH_PROPERTY_BIT_MASK 0xFFFFFFFF
#define FRAME_GRAPH_NODE_BIT_MASK 0xFFFF0000
#define FRAME_GRAPH_PROPERTY_BIT_COUNT 16
#define FRAME_GRAPH_NODE_PROPERTY_BIT_COUNT 32

enum class PropertyType
{
    DrawList,
    Image,
    Float // float
};

class Node;
class Property
{
public:
    Property(Node* parent, const char* name, PropertyType type, void* data, FGID id, bool isInput)
        : parent(parent), name(name), type(type), data(data), id(id), isInput(isInput)
    {}

    PropertyType GetType() const
    {
        return type;
    }

    const std::string& GetName() const
    {
        return name;
    }

    uint32_t GetID()
    {
        return id;
    }

    Node* GetParent()
    {
        return parent;
    }

    void* GetData()
    {
        return data;
    }

    bool IsOuput()
    {
        return !isInput;
    }

    bool IsInput()
    {
        return isInput;
    }

protected:
    Node* parent;
    std::string name;
    PropertyType type;
    void* const data;
    uint32_t id;
    bool isInput;
};

class Node
{
public:
    Node(const char* name, FGID id) : id(id), name(name) {}
    FGID GetID()
    {
        return id;
    }

    std::span<Property> GetInput()
    {
        return inputProperties;
    }
    std::span<Property> GetOutput()
    {
        return outputProperties;
    }

    Property* GetProperty(FGID id)
    {
        for (auto& p : inputProperties)
        {
            if (p.GetID() == id)
                return &p;
        }

        for (auto& p : outputProperties)
        {
            if (p.GetID() == id)
                return &p;
        }

        return nullptr;
    }

    const std::string GetName() const
    {
        return name;
    }

protected:
    void AddInputProperty(const char* name, PropertyType type, void* data)
    {
        inputProperties.emplace_back(
            this,
            name,
            type,
            data,
            GetID() + (inputProperties.size() + outputProperties.size() + 1),
            true
        ); // plus one to avoid the same id as node itself
    }

    void AddOutputProperty(const char* name, PropertyType type, void* data)
    {
        outputProperties.emplace_back(
            this,
            name,
            type,
            data,
            GetID() + (inputProperties.size() + outputProperties.size() + 1),
            false
        ); // plus one to avoid the same id as node itself
    }

private:
    std::vector<Property> inputProperties;
    std::vector<Property> outputProperties;

private:
    FGID id;
    std::string name;
};
} // namespace Engine::FrameGraph
