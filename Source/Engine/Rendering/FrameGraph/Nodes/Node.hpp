#pragma once
#include "../GraphResource.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <any>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <vector>
namespace Engine
{
class MeshRenderer;
}
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

enum class ConfigurableType
{
    Bool,
    Int,
    Float,
    Format,
    Vec2,
    Vec3,
    Vec4,
    Vec2Int,
    Vec3Int,
    Vec4Int,
    ObjectPtr,
};

struct SceneObjectDrawData
{
    SceneObjectDrawData() = default;
    SceneObjectDrawData(SceneObjectDrawData&& other) = default;
    Gfx::ShaderProgram* shader = nullptr;
    const Gfx::ShaderConfig* shaderConfig = nullptr;
    Gfx::ShaderResource* shaderResource = nullptr;
    Gfx::Buffer* indexBuffer = nullptr;
    Gfx::IndexBufferType indexBufferType;
    std::vector<Gfx::VertexBufferBinding> vertexBufferBinding;
    glm::mat4 pushConstant;
    uint32_t indexCount;
};
class DrawList : public std::vector<SceneObjectDrawData>
{
public:
    void Add(MeshRenderer& meshRenderer);
};

struct Configurable
{
    Configurable(const char* name, ConfigurableType type, std::any&& defaultVal)
        : name(name), type(type), data(defaultVal)
    {}
    Configurable(Configurable&& other) = default;
    // type checked constructor
    template <ConfigurableType type, class T>
    static Configurable C(const char* name, const T& val)
    {
        if constexpr (type == ConfigurableType::Bool)
            static_assert(std::is_same_v<T, bool>);
        if constexpr (type == ConfigurableType::Int)
            static_assert(std::is_same_v<T, int>);
        else if constexpr (type == ConfigurableType::Float)
            static_assert(std::is_same_v<T, float>);
        else if constexpr (type == ConfigurableType::Format)
            static_assert(std::is_same_v<T, Gfx::ImageFormat>);
        else if constexpr (type == ConfigurableType::Vec2)
            static_assert(std::is_same_v<T, glm::vec2>);
        else if constexpr (type == ConfigurableType::Vec3)
            static_assert(std::is_same_v<T, glm::vec3>);
        else if constexpr (type == ConfigurableType::Vec4)
            static_assert(std::is_same_v<T, glm::vec4>);
        else if constexpr (type == ConfigurableType::Vec2Int)
            static_assert(std::is_same_v<T, glm::ivec2>);
        else if constexpr (type == ConfigurableType::Vec3Int)
            static_assert(std::is_same_v<T, glm::ivec3>);
        else if constexpr (type == ConfigurableType::Vec4Int)
            static_assert(std::is_same_v<T, glm::ivec4>);
        else if constexpr (type == ConfigurableType::ObjectPtr)
        {
            static_assert(std::is_pointer_v<T> || std::is_null_pointer_v<T>);
            if constexpr (std::is_null_pointer_v<T>)
            {

                // make sure user can safely cast the data into an Object* object
                // if we don't cast and val is a nullptr-t, it will throw bad_any_cast
                return Configurable{name, type, (Object*)nullptr};
            }
        }

        return Configurable{name, type, val};
    }

    std::string name;
    ConfigurableType type;
    mutable std::any data;

private:
    // used when deserializing this class and type is ObjectPtr
    Object* dataRefHolder;

    friend class Node;
};

class Node;
enum class PropertyType
{
    DrawList,
    RenderGraphLink,
    Float // float
};
class Property
{
public:
    Property(Node* parent, const char* name, PropertyType type, FGID id, bool isInput)
        : parent(parent), name(name), type(type), id(id), isInput(isInput)
    {}

    PropertyType GetType() const
    {
        return type;
    }

    const std::string& GetName() const
    {
        return name;
    }

    FGID GetID()
    {
        return id;
    }

    Node* GetParent()
    {
        return parent;
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
    FGID id;
    bool isInput;

    friend class Node;
};

enum class ResourceType
{
    RenderGraphLink,
    DrawList,
    Forwarding,
};
namespace ResourceTag
{
struct RenderGraphLink
{};
struct DrawList
{};

// this resource doesn't actually reference a actual resource but rather reference to a property id that may contain or
// may forward again another resource
struct Forwarding
{};
} // namespace ResourceTag
  //
struct Resource
{
    Resource(
        ResourceTag::RenderGraphLink, FGID propertyID, RenderGraph::RenderNode* node, RenderGraph::ResourceHandle handle
    )
        : type(ResourceType::RenderGraphLink), propertyID(propertyID), node(node), handle(handle)
    {}
    Resource(ResourceTag::DrawList, FGID propertyID, const DrawList* drawList)
        : type(ResourceType::DrawList), propertyID(propertyID), drawList(drawList)
    {}

    ResourceType type;
    FGID propertyID;

    // RenderGraphLink
    RenderGraph::RenderNode* node;
    RenderGraph::ResourceHandle handle;

    const DrawList* drawList;
};

class Resources
{
public:
    std::tuple<RenderGraph::RenderNode*, RenderGraph::ResourceHandle> GetResource(ResourceTag::RenderGraphLink, FGID id)
    {
        auto iter = resources.find(id);
        if (iter != resources.end())
        {
            if (iter->second.type != ResourceType::RenderGraphLink)
            {
                throw std::logic_error("mismatched type");
            }

            return {iter->second.node, iter->second.handle};
        }

        return {nullptr, -1};
    }
    const DrawList* GetResource(ResourceTag::DrawList, FGID id)
    {
        auto iter = resources.find(id);
        if (iter != resources.end())
        {
            if (iter->second.type == ResourceType::Forwarding)
            {
                throw std::logic_error("not handled");
            }

            return iter->second.drawList;
        }

        return nullptr;
    }

private:
    std::unordered_map<FGID, Resource> resources;

    friend class Graph;
};

class Node : public Object, public Serializable
{
public:
    Node(){};
    Node(const char* name, FGID id) : id(id), name(name), customName(name) {}
    Node(Node&& other) = default;

    FGID GetID()
    {
        return id;
    }

    virtual std::vector<Resource> Preprocess(RenderGraph::Graph& graph)
    {
        return {};
    }
    virtual void Build(RenderGraph::Graph& graph, Resources& resources){};
    virtual void Finalize(RenderGraph::Graph& graph, Resources& resources){};
    virtual void ProcessSceneShaderResource(Gfx::ShaderResource& sceneShaderResource){};
    virtual void Execute(GraphResource& graphResource){};
    virtual void OnDestroy() {}
    std::span<Property> GetInput()
    {
        return inputProperties;
    }
    std::span<Property> GetOutput()
    {
        return outputProperties;
    }

    std::span<const Configurable> GetConfiurables()
    {
        return configs;
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

    void SetCustomName(const char* name)
    {
        customName = name;
    }

    const std::string& GetCustomName()
    {
        return customName;
    }

    const std::string GetName() const
    {
        return name;
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

protected:
    template <class T>
    T GetConfigurableVal(const char* name)
    {
        for (auto& c : configs)
        {
            if (strcmp(c.name.c_str(), name) == 0)
            {
                if constexpr (std::is_pointer_v<T>)
                    return dynamic_cast<T>(std::any_cast<Object*>(c.data));
                else
                    return std::any_cast<T>(c.data);
            }
        }

        return T{};
    }

    // Get a pointer points to the value of the Configurable
    template <class T>
    T* GetConfigurablePtr(const char* name)
    {
        for (auto& c : configs)
        {
            if (strcmp(c.name.c_str(), name) == 0)
            {
                if constexpr (std::is_pointer_v<T>)
                    return static_cast<T*>(std::any_cast<Object*>(&c.data));
                else
                    return std::any_cast<T>(&c.data);
            }
        }

        return nullptr;
    }

    FGID AddInputProperty(const char* name, PropertyType type)
    {
        FGID id = GetID() + (inputProperties.size() + outputProperties.size() + 1);
        inputProperties.emplace_back(this, name, type, id,
                                     true); // plus one to avoid the same id as node itself

        inputPropertyIDs[name] = id;

        return id;
    }

    FGID AddOutputProperty(const char* name, PropertyType type)
    {
        FGID id = GetID() + (inputProperties.size() + outputProperties.size() + 1);
        outputProperties.emplace_back(this, name, type, id,
                                      false); // plus one to avoid the same id as node itself

        outputPropertyIDs[name] = id;

        return id;
    }

    template <ConfigurableType type, class T>
    void AddConfig(const char* name, const T& val)
    {
        configs.emplace_back(Configurable::C<type>(name, val));
    }

    std::unordered_map<std::string, FGID> inputPropertyIDs;
    std::unordered_map<std::string, FGID> outputPropertyIDs;

private:
    std::vector<Property> inputProperties;
    std::vector<Property> outputProperties;
    FGID id = 0;
    std::string name;
    std::string customName;
    std::vector<Configurable> configs;
};
} // namespace Engine::FrameGraph
