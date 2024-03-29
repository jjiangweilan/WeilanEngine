#include "FrameGraph.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/GameObject.hpp"
#include "Nodes/ImageNode.hpp"
#include <spdlog/spdlog.h>

namespace Engine::FrameGraph
{
DEFINE_ASSET(Graph, "C18AC918-98D0-41BF-920D-DE0FD7C06029", "fgraph");

Node& Graph::AddNode(const NodeBlueprint& bp)
{
    SetDirty();
    auto n = bp.CreateNode(nodeIDPool.Allocate() << FRAME_GRAPH_PROPERTY_BIT_COUNT);
    auto t = n.get();
    nodes.push_back(std::move(n));
    return *t;
}

bool Graph::Connect(FGID src, FGID dst)
{
    SetDirty();

    if (src == dst)
        return false;

    FGID srcNodeID = GetNodeID(src);
    FGID dstNodeID = GetNodeID(dst);

    auto srcIter = std::find_if(
        nodes.begin(),
        nodes.end(),
        [srcNodeID](std::unique_ptr<Node>& n) { return n->GetID() == srcNodeID; }
    );
    auto dstIter = std::find_if(
        nodes.begin(),
        nodes.end(),
        [dstNodeID](std::unique_ptr<Node>& n) { return n->GetID() == dstNodeID; }
    );
    if (srcIter == nodes.end() || dstIter == nodes.end() || srcIter == dstIter)
        return false;

    std::unique_ptr<Node>& srcNode = *srcIter;
    std::unique_ptr<Node>& dstNode = *dstIter;

    Property* srcProp = srcNode->GetProperty(src);
    Property* dstProp = dstNode->GetProperty(dst);

    if (srcProp == nullptr || dstProp == nullptr)
        return false;
    if ((srcProp->GetType() != dstProp->GetType()) || (srcProp->IsOuput() && dstProp->IsOuput()) ||
        (srcProp->IsInput() && dstProp->IsInput()))
        return false;

    if (HasCycleIfLink(src, dst))
    {
        return false;
    }

    connections.push_back(src << FRAME_GRAPH_NODE_PROPERTY_BIT_COUNT | dst);

    return true;
}

void Graph::DeleteNode(Node* node)
{
    RequireRecompile();
    SetDirty();

    auto nodeIter =
        std::find_if(nodes.begin(), nodes.end(), [node](std::unique_ptr<Node>& n) { return n.get() == node; });
    if (nodeIter == nodes.end())
    {
        throw std::logic_error("Deleted a non-existing node");
    }
    FGID nodeID = node->GetID();
    node->OnDestroy();
    nodes.erase(nodeIter);
    nodeIDPool.Release(nodeID >> FRAME_GRAPH_PROPERTY_BIT_COUNT);

    auto iter = std::remove_if(
        connections.begin(),
        connections.end(),

        [nodeID](FGID& c)
        {
            FGID srcNode = GetSrcNodeIDFromConnect(c);
            FGID dstNode = GetDstNodeIDFromConnect(c);
            return srcNode == nodeID || dstNode == nodeID;
        }
    );

    connections.erase(iter, connections.end());
}

void Graph::DeleteConnection(FGID connectionID)
{
    SetDirty();

    auto iter = std::find(connections.begin(), connections.end(), connectionID);
    if (iter != connections.end())
    {
        connections.erase(iter);
    }
}

std::span<FGID> Graph::GetConnections()
{
    return connections;
}

void Graph::DeleteNode(FGID id)
{
    auto iter = std::find_if(nodes.begin(), nodes.end(), [id](std::unique_ptr<Node>& n) { return n->GetID() == id; });
    DeleteNode(iter->get());
}

void Graph::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("nodeIDPool", nodeIDPool);
    s->Serialize("connections", connections);
    s->Serialize("nodes", nodes);
    s->Serialize("outputImageNode", outputImageNode->GetID());
    s->Serialize("templateSceneResourceShader", templateSceneResourceShader);
}

void Graph::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("nodeIDPool", nodeIDPool);
    s->Deserialize("connections", connections);
    s->Deserialize("nodes", nodes);
    FGID outputImageNodeID;
    s->Deserialize("outputImageNode", outputImageNodeID);
    for (auto& n : nodes)
    {
        if (n->GetID() == outputImageNodeID)
        {
            outputImageNode = n.get();
            break;
        }
    }
    s->Deserialize("templateSceneResourceShader", templateSceneResourceShader);
}

void Graph::ReportValidation()
{
    std::unordered_map<FGID, int> counts;
    for (auto c : connections)
    {
        counts[c] += 1;
    }

    for (auto& n : nodes)
    {
        counts[n->GetID()] += 1;
        for (auto& p : n->GetInput())
        {
            counts[p.GetID()] += 1;
        }

        for (auto& p : n->GetOutput())
        {
            counts[p.GetID()] += 1;
        }
    }

    for (auto& iter : counts)
    {
        spdlog::info("{}, {}", iter.first, iter.second);
    }
}
void Graph::ProcessLights(Scene& gameScene)
{
    auto lights = gameScene.GetActiveLights();

    Light* light = nullptr;
    for (int i = 0; i < lights.size(); ++i)
    {
        sceneInfo.lights[i].intensity = lights[i]->GetIntensity();
        auto model = lights[i]->GetGameObject()->GetTransform()->GetModelMatrix();
        sceneInfo.lights[i].position = glm::vec4(-glm::normalize(glm::vec3(model[2])), 0);

        if (lights[i]->GetLightType() == LightType::Directional)
        {
            if (light == nullptr || light->GetIntensity() < lights[i]->GetIntensity())
            {
                light = lights[i];
            }
        }
    }

    sceneInfo.lightCount = glm::vec4(lights.size(), 0, 0, 0);
    if (light)
    {
        sceneInfo.worldToShadow = light->WorldToShadowMatrix();
    }
}

void Graph::Execute(Gfx::CommandBuffer& cmd, Scene& scene)
{
    if (!compiled)
        return;

    Camera* camera = scene.GetMainCamera();

    if (camera == nullptr)
        return;

    graphResource.mainCamera = camera;

    for (auto& n : nodes)
    {
        n->Execute(graphResource);
    }

    RefPtr<Transform> camTsm = camera->GetGameObject()->GetTransform();

    glm::mat4 viewMatrix = camera->GetViewMatrix();
    glm::mat4 projectionMatrix = camera->GetProjectionMatrix();
    glm::mat4 vp = projectionMatrix * viewMatrix;
    glm::vec4 viewPos = glm::vec4(camTsm->GetPosition(), 1);
    sceneInfo.projection = projectionMatrix;
    sceneInfo.viewProjection = vp;
    sceneInfo.viewPos = viewPos;
    sceneInfo.view = viewMatrix;
    ProcessLights(scene);

    size_t copySize = sceneGlobalBuffer->GetSize();
    Gfx::BufferCopyRegion regions[] = {{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = copySize,
    }};

    memcpy(stagingBuffer->GetCPUVisibleAddress(), &sceneInfo, sizeof(sceneInfo));
    cmd.CopyBuffer(stagingBuffer, sceneGlobalBuffer, regions);

    cmd.BindResource(0, sceneShaderResource.get());
    Gfx::GPUBarrier barrier{
        .buffer = sceneGlobalBuffer,
        .srcStageMask = Gfx::PipelineStage::Transfer,
        .dstStageMask = Gfx::PipelineStage::Vertex_Shader | Gfx::PipelineStage::Fragment_Shader,
        .srcAccessMask = Gfx::AccessMask::Transfer_Write,
        .dstAccessMask = Gfx::AccessMask::Memory_Read,
    };

    cmd.Barrier(&barrier, 1);
    graph->Execute(cmd);
}

bool Graph::Compile()
{
    GetGfxDriver()->WaitForIdle();

    sceneShaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource();
    stagingBuffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Src,
        .size = 1024 * 1024, // 1 MB
        .visibleInCPU = true,
        .debugName = "dual moon graph staging buffer",
    });
    sceneGlobalBuffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
        .size = sizeof(SceneInfo),
        .visibleInCPU = false,
        .debugName = "Scene Info Buffer",
    });
    sceneShaderResource->SetBuffer("SceneInfo", sceneGlobalBuffer.get());

    graph = std::make_unique<RenderGraph::Graph>();

    Resources buildResources;

    for (auto& n : nodes)
    {
        auto resources = n->Preprocess(*graph);
        for (auto& r : resources)
        {
            if (r.type == ResourceType::Forwarding)
            {
                throw std::runtime_error("Not implemented");
            }
            else
            {
                for (auto c : connections)
                {
                    if (GetSrcPropertyIDFromConnectionID(c) == r.propertyID)
                    {
                        buildResources.resources.emplace(GetDstPropertyIDFromConnectionID(c), r);
                    }
                }
            }
        }
    }

    for (auto& n : nodes)
    {
        if (!n->Build(*graph, buildResources))
        {
            compiled = false;
            return compiled;
        }
    }

    graph->Process();

    for (auto& n : nodes)
    {
        n->ProcessSceneShaderResource(*sceneShaderResource);
    }

    for (auto& n : nodes)
    {
        n->Finalize(*graph, buildResources);
    }

    compiled = true;
    return compiled;
}

void Graph::SetOutputImageNode(FGID nodeID)
{
    for (auto& n : nodes)
    {
        if (n->GetID() == nodeID)
        {
            if (n->GetObjectTypeID() == ImageNode::StaticGetObjectTypeID())
            {
                outputImageNode = n.get();
                SetDirty();
                return;
            }
        }
    }
}

Node* Graph::GetNode(FGID nodeID)
{
    for (auto& n : nodes)
    {
        if (n->GetID() == nodeID)
            return n.get();
    }

    return nullptr;
}

Gfx::Image* Graph::GetOutputImage()
{
    if (this->outputImageNode == nullptr || !compiled)
    {
        return nullptr;
    }

    if (ImageNode* outputImageNode = dynamic_cast<ImageNode*>(this->outputImageNode))
    {
        return outputImageNode->GetImage();
    }

    return nullptr;
}
} // namespace Engine::FrameGraph
