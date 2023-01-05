#pragma once
#include "Libs/UUID.hpp"
#include <cassert>
#include <typeinfo>

namespace Engine::RGraph
{
class Node;

class Resource
{};
/**
 * a port can either input or output a ResourceRef, this is a wrapper around
 * that resource, a resource reference can exist before the actual resource is created
 */
class ResourceRef
{
public:
    ResourceRef(const std::type_info& type) : typeInfo(&type), val(nullptr), uuid() {}
    template <class T>
    ResourceRef(T* val) : typeInfo(&typeid(T)), val(val), uuid()
    {}
    ResourceRef(const ResourceRef& other) : typeInfo(other.typeInfo), val(other.val), uuid(other.uuid) {}
    ResourceRef& operator=(const ResourceRef& other)
    {
        this->typeInfo = other.typeInfo;
        this->val = other.val;
        this->uuid = other.uuid;

        return *this;
    }

    bool operator==(const ResourceRef& other) const { return typeInfo == other.typeInfo && uuid == other.uuid; }

    bool IsType(const std::type_info& type)
    {
        if (typeInfo) return type == *typeInfo;
        return false;
    };
    void* GetVal() { return val; }
    template <class T>
    void SetVal(T* val)
    {
        assert(typeid(T) == *typeInfo);
        this->val = val;
    }

    const UUID& GetUUID() const { return uuid; }

private:
    const std::type_info* typeInfo;
    void* val;
    UUID uuid;
};

class Port
{
public:
    enum class Type
    {
        Input,
        Output
    };
    enum class ConnectionType
    {
        Connect,
        Disconnect
    };

    using ConnectionCallBack = std::function<void(Node* node, Port* port, ConnectionType type)>;

    Port(Node* parent, const char* name, Type type, const std::type_info& resourceType, bool isMultiPort = false,
         ConnectionCallBack connectionCallback = nullptr)
        : parent(parent), resourceRefs(), type(type), resourceType(resourceType), name(name), isMultiPort(isMultiPort),
          connectionCallback(connectionCallback)
    {
        if (type == Type::Output) assert(isMultiPort == false);
    }
    virtual ~Port() {}

    const std::type_info& GetResourceType() { return resourceType; }

    Type GetType() { return type; }

    /**
     * Get the value that is passed through the port, template is used for type
     * check
     *
     */
    ResourceRef* GetResource()
    {
        assert(!isMultiPort);
        if (!isMultiPort && !resourceRefs.empty())
        {
            return resourceRefs[0];
        }

        return nullptr;
    }

    void* GetResourceVal()
    {
        assert(!isMultiPort);

        if (!resourceRefs.empty()) return resourceRefs[0]->GetVal();
        return nullptr;
    }

    std::span<ResourceRef*> GetResources()
    {
        assert(isMultiPort);
        return resourceRefs;
    }

    void RemoveResource(ResourceRef* res)
    {
        auto iter = std::find(resourceRefs.begin(), resourceRefs.end(), res);
        if (iter != resourceRefs.end())
        {
            resourceRefs.erase(iter);
        }
    }

    void SetResource(ResourceRef* resource)
    {
        assert(!isMultiPort);

        auto iter = std::find(resourceRefs.begin(), resourceRefs.end(), resource);
        if (iter != resourceRefs.end())
        {
            return;
        }

        if (resourceRefs.empty()) this->resourceRefs.push_back(resource);
        else this->resourceRefs[0] = resource;
    }

    void AddResource(ResourceRef* resource)
    {
        assert(isMultiPort);

        auto iter = std::find(resourceRefs.begin(), resourceRefs.end(), resource);
        if (iter != resourceRefs.end())
        {
            return;
        }

        this->resourceRefs.push_back(resource);
    }

    bool CanConnect(Port* port)
    {
        if (!isMultiPort && !connected.empty()) return false;
        return resourceType == port->resourceType;
    }
    bool IsMultiPort() { return isMultiPort; }

    Node* GetNode() { return parent; }
    std::span<Port*> GetConnectedPorts() { return connected; }
    Port* GetConnectedPort()
    {
        if (!connected.empty())
        {
            assert(!isMultiPort && "Use GetConnectedPorts to avoid runtime error");
            return connected[0];
        }
        return nullptr;
    }
    void Connect(Port* other);
    void Disconnect(Port* other);

protected:
    Node* parent;
    std::vector<Port*> connected;
    std::vector<ResourceRef*> resourceRefs;
    Type type;
    const std::type_info& resourceType;
    std::string name;
    bool isMultiPort;
    ConnectionCallBack connectionCallback;

    friend class NodeGraph;
};
} // namespace Engine::RGraph
