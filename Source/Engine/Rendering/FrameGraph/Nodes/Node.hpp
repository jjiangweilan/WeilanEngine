#pragma once
#include <span>
#include <string>
#include <vector>

namespace Engine::FrameGraph
{
using NodeID = int;

// reverse 999 properties for each node, should be very enough
#define RESERVED_PROPERTY_ID 1000

enum class PropertyType
{
    DrawList,
    Image,
    Float // float
};

class Property
{
public:
    Property(const char* name, PropertyType type, void* data, uint32_t id) : name(name), type(type), data(data), id(id)
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

    void* GetData()
    {
        return data;
    }

protected:
    std::string name;
    PropertyType type;
    void* const data;
    uint32_t id;
};

class Node
{
public:
    Node(const char* name, NodeID id) : id(id * RESERVED_PROPERTY_ID), name(name) {}
    NodeID GetID()
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

    const std::string GetName() const
    {
        return name;
    }

protected:
    void AddInputProperty(const char* name, PropertyType type, void* data)
    {
        inputProperties.emplace_back(name, type, data, (uint32_t)(inputProperties.size() + outputProperties.size()));
    }

    void AddOutputProperty(const char* name, PropertyType type, void* data)
    {
        outputProperties.emplace_back(name, type, data, (uint32_t)inputProperties.size() + outputProperties.size());
    }

private:
    std::vector<Property> inputProperties;
    std::vector<Property> outputProperties;

private:
    NodeID id;
    std::string name;
};
} // namespace Engine::FrameGraph
