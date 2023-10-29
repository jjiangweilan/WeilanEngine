#pragma once
#include <span>
#include <string>
#include <vector>

namespace Engine::FrameGraph
{
using NodeID = int;

enum class PropertyType
{
    Image,
    Number
};

struct InputProperty
{};

struct OutputProperty
{};

class Node
{
public:
    Node(const char* name, NodeID id) : id(id), name(name) {}
    NodeID GetID()
    {
        return id;
    }

    std::span<InputProperty> GetInput()
    {
        return inputProperties;
    }
    std::span<OutputProperty> GetOutput()
    {
        return outputProperties;
    }

    const std::string GetName() const
    {
        return name;
    }

private:
    std::vector<InputProperty> inputProperties;
    std::vector<OutputProperty> outputProperties;

private:
    NodeID id;
    std::string name;
};
} // namespace Engine::FrameGraph
