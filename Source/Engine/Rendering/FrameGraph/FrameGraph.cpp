#include "FrameGraph.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/GameObject.hpp"
#include "Core/Texture.hpp"
#include "Core/Time.hpp"
#include "GfxDriver/Image.hpp"
#include "Nodes/ImageNode.hpp"
#include <spdlog/spdlog.h>
#include <stack>

#include "Profiler/Profiler.hpp"
#include "Rendering/Graphics.hpp"

namespace Rendering::FrameGraph
{
DEFINE_ASSET(Graph, "C18AC918-98D0-41BF-920D-DE0FD7C06029", "fgraph");

Graph::Graph()
{

    SetName("New Frame Graph");
#if ENGINE_EDITOR
    ax::NodeEditor::Config config;
    config.SettingsFile = "Frame Graph Editor.json";
    graphContext = ax::NodeEditor::CreateEditor(&config);
#endif

    brdfPreintegral = (Texture*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Textures/BRDFPreintegral.ktx");
}

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
    FGID id = 0;
    if (outputImageNode)
        id = outputImageNode->GetID();
    s->Serialize("outputImageNode", id);
    if (outputDepthImageNode)
        id = outputDepthImageNode->GetID();
    s->Serialize("outputDepthImageNode", id);
}

void Graph::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("nodeIDPool", nodeIDPool);
    s->Deserialize("connections", connections);
    s->Deserialize("nodes", nodes);
    FGID outputImageNodeID;
    s->Deserialize("outputImageNode", outputImageNodeID);
    FGID outputDepthImageNodeID;
    s->Deserialize("outputDepthImageNode", outputDepthImageNodeID);
    for (auto& n : nodes)
    {
        if (n->GetID() == outputDepthImageNodeID)
        {
            outputDepthImageNode = n.get();
            break;
        }
    }
    for (auto& n : nodes)
    {
        if (n->GetID() == outputImageNodeID)
        {
            outputImageNode = n.get();
            break;
        }
    }

    // TODO: we can fix invalid connections with the following code, and it lets compiling FrameGraph through.
    // In that case, the invalid connections are disconnected, but we havn't yet have input validation check when
    // Executing the graph, hence causing runtime error. runtime validation check of inputs should be implemented before
    // we can auto disconnect invalid connections when deserializing auto connectionCopy = connections;
    // connections.clear();
    // for(auto c : connectionCopy)
    // {
    //     Connect(c >> 32, c & 0xffffffff);
    // }
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
void Graph::ProcessLights(Scene& gameScene, Light*& mainLight)
{
    auto lights = gameScene.GetActiveLights();

    for (int i = 0; i < lights.size(); ++i)
    {
        sceneInfo.lights[i].ambientScale = lights[i]->GetAmbientScale();
        sceneInfo.lights[i].lightColor = glm::vec4(lights[i]->GetLightColor(), 1.0);
        sceneInfo.lights[i].intensity = lights[i]->GetIntensity();
        auto model = lights[i]->GetGameObject()->GetWorldMatrix();
        switch (lights[i]->GetLightType())
        {
            case LightType::Directional:
                {
                    mainLight = lights[i];
                    glm::vec3 pos = -glm::normalize(glm::vec3(model[2]));
                    sceneInfo.lights[i].position = {pos, 0};

                    if (mainLight == nullptr || mainLight->GetIntensity() < lights[i]->GetIntensity())
                    {
                        mainLight = lights[i];
                    }
                    break;
                }
            case LightType::Point:
                {
                    glm::vec3 pos = model[3];
                    sceneInfo.lights[i].position = {pos, 1};
                    sceneInfo.lights[i].pointLightTerm1 = lights[i]->GetPointLightLinear();
                    sceneInfo.lights[i].pointLightTerm2 = lights[i]->GetPointLightDistance();
                    break;
                }
        }
    }

    sceneInfo.lightCount = lights.size();
}
static void SortNodesInternal(
    FGID visit,
    std::unordered_map<FGID, std::vector<FGID>>& fromNodeToNode,
    std::unordered_map<FGID, bool>& visited,
    std::stack<FGID>& stack
)
{
    visited[visit] = true;

    for (auto& id : fromNodeToNode[visit])
    {
        if (!visited[id])
            SortNodesInternal(id, fromNodeToNode, visited, stack);
    }

    stack.push(visit);
}

void Graph::SortNodes()
{
    std::unordered_map<FGID, std::vector<FGID>> fromNodeToNode;
    for (FGID connection : connections)
    {
        FGID fromNode = GetSrcNodeIDFromConnect(connection);
        FGID toNode = GetDstNodeIDFromConnect(connection);

        fromNodeToNode[fromNode].push_back(toNode);
    }

    std::unordered_map<FGID, bool> visited;
    for (auto& n : fromNodeToNode)
    {
        visited[n.first] = false;
    }

    bool allFinished = true;
    std::stack<FGID> sorted;
    do
    {
        allFinished = true;
        for (auto& n : visited)
        {
            if (n.second == false)
            {
                allFinished = false;
                SortNodesInternal(n.first, fromNodeToNode, visited, sorted);
                break;
            }
        }
    }
    while (!allFinished);

    this->sortedNodes.clear();
    while (sorted.empty() == false)
    {
        this->sortedNodes.push_back(GetNode(sorted.top()));
        sorted.pop();
    }
}

void Graph::Execute(Gfx::CommandBuffer& cmd, Scene& scene, Camera& camera)
{
    if (!compiled)
        return;
    ENGINE_BEGIN_PROFILE("FrameGraph: Prepare");
    renderingData.mainCamera = &camera;
    renderingData.renderingScene = &scene.GetRenderingScene();
    renderingData.sceneInfo = &sceneInfo;
    renderingData.cmd = &cmd;

    // auto sceneEnvironment = scene.GetRenderingScene().GetSceneEnvironment();

    cmd.SetBuffer("SceneInfo", *sceneInfoBuffer);
    auto diffuseCube = camera.GetDiffuseEnv();
    auto specularCube = camera.GetSpecularEnv();
    if (diffuseCube)
        cmd.SetTexture("diffuseCube", *diffuseCube->GetGfxImage());
    if (specularCube)
        cmd.SetTexture("specularCube", *specularCube->GetGfxImage());

    if (brdfPreintegral)
        cmd.SetTexture("specularBRDFIntegrationMap", *brdfPreintegral->GetGfxImage());

    renderingData.terrain = scene.GetRenderingScene().GetTerrain();
    GameObject* camGo = camera.GetGameObject();

    glm::mat4 viewMatrix = camera.GetViewMatrix();
    glm::mat4 projectionMatrix = camera.GetProjectionMatrix();
    glm::mat4 vp = projectionMatrix * viewMatrix;
    glm::vec4 viewPos = glm::vec4(camGo->GetPosition(), 1);
    sceneInfo.projection = projectionMatrix;
    sceneInfo.viewProjection = vp;
    sceneInfo.viewPos = viewPos;
    sceneInfo.view = viewMatrix;
    sceneInfo.shadowMapSize = {
        1024,
        1024,
        1.0 / 1024.0f,
        1.0 / 1024.0f,
    };
    sceneInfo.invProjection = glm::inverse(projectionMatrix);
    sceneInfo.invNDCToWorld = glm::inverse(viewMatrix) * glm::inverse(projectionMatrix);
    sceneInfo.cameraZBufferParams = glm::vec4(
        camera.GetNear(),
        camera.GetFar(),
        (camera.GetNear() - camera.GetFar()) / (camera.GetNear() * camera.GetFar()),
        1.0f / camera.GetNear()
    );
    sceneInfo.cameraFrustum = glm::vec4(
        -camera.GetProjectionRight(),
        camera.GetProjectionRight(),
        -camera.GetProjectionTop(),
        camera.GetProjectionTop()
    );
    sceneInfo.screenSize = glm::vec4(
        renderingData.screenSize.x,
        renderingData.screenSize.y,
        1.0f / renderingData.screenSize.x,
        1.0f / renderingData.screenSize.y
    );
    Light* mainLight = nullptr;
    ProcessLights(scene, mainLight);
    if (mainLight)
    {
        renderingData.updateMainLightShadow = mainLight->ShouldRenderShadowMap();

        sceneInfo.worldToShadow =
            mainLight->WorldToShadowMatrix(renderingData.mainCamera->GetGameObject()->GetPosition());

        if (mainLight->IsShadowCacheEnabled())
        {
            sceneInfo.cachedMainLightDirection = glm::vec4(mainLight->GetCachedLightDirection(), 1.0f);
        }
        else
        {
            sceneInfo.cachedMainLightDirection = glm::vec4(mainLight->GetLightDirection(), 0.0f);
        }
    }

    size_t copySize = sceneInfoBuffer->GetSize();
    Gfx::BufferCopyRegion regions[] = {{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = copySize,
    }};

    memcpy(stagingBuffer->GetCPUVisibleAddress(), &sceneInfo, sizeof(sceneInfo));
    cmd.CopyBuffer(stagingBuffer, sceneInfoBuffer, regions);

    shaderGlobal.time = Time::TimeSinceLaunch();
    GetGfxDriver()->UploadBuffer(*shaderGlobalBuffer, (uint8_t*)&shaderGlobal, sizeof(ShaderGlobal));
    cmd.SetBuffer("Global", *shaderGlobalBuffer);
    ENGINE_END_PROFILE

    for (auto& n : sortedNodes)
    {
        ENGINE_BEGIN_PROFILE(fmt::format("FrameGraph: {}", n->GetCustomName()));
        float color[4] = {0.235294f, 0.882353f, 0.941176f, 1.0f};
        cmd.BeginLabel(n->GetCustomName(), color);
        n->Execute(renderingContext, renderingData);
        cmd.EndLabel();
        ENGINE_END_PROFILE
    }
}

bool Graph::Compile()
{
    GetGfxDriver()->WaitForIdle();

    stagingBuffer = GetGfxDriver()->CreateBuffer({
        .usages = Gfx::BufferUsage::Transfer_Src,
        .size = 1024 * 1024, // 1 MB
        .visibleInCPU = true,
        .debugName = "frame graph staging buffer",
    });
    sceneInfoBuffer = GetGfxDriver()->CreateBuffer(
        {.usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
         .size = sizeof(SceneInfo),
         .visibleInCPU = false,
         .debugName = "Scene Info Buffer",
         .gpuWrite = true}
    );
    shaderGlobalBuffer = GetGfxDriver()->CreateBuffer(
        {.usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
         .size = sizeof(SceneInfo),
         .visibleInCPU = false,
         .debugName = "Shader Global Buffer",
         .gpuWrite = true}
    );

    SortNodes();

    for (auto c : connections)
    {
        auto srcNode = GetNode(GetSrcNodeIDFromConnect(c));
        auto srcProperty = srcNode->GetProperty(GetSrcPropertyIDFromConnectionID(c));

        auto dstNode = GetNode(GetDstNodeIDFromConnect(c));
        bool validLink = dstNode->GetProperty(GetDstPropertyIDFromConnectionID(c))->LinkFromOutput(*srcProperty);
        if (!validLink)
        {
            compiled = false;
            return compiled;
        }
    }

    for (auto n : sortedNodes)
    {
        n->Compile();
    }

    compiled = true;
    return compiled;
}

void Graph::SetOutputDepthImageNode(FGID nodeID)
{
    for (auto& n : nodes)
    {
        if (n->GetID() == nodeID)
        {
            if (n->GetObjectTypeID() == ImageNode::StaticGetObjectTypeID())
            {
                outputDepthImageNode = n.get();
                SetDirty();
                return;
            }
        }
    }
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

const Gfx::RG::ImageIdentifier* Graph::GetOutputImage()
{
    if (this->outputImageNode == nullptr || !compiled)
    {
        return nullptr;
    }

    if (ImageNode* outputImageNode = dynamic_cast<ImageNode*>(this->outputImageNode))
    {
        return &outputImageNode->GetImage();
    }

    return nullptr;
}

const Gfx::RG::ImageIdentifier* Graph::GetOutputDepthImage()
{
    if (this->outputDepthImageNode == nullptr || !compiled)
    {
        return nullptr;
    }

    if (ImageNode* outputImageNode = dynamic_cast<ImageNode*>(this->outputDepthImageNode))
    {
        Gfx::Image* outputImage = GetGfxDriver()->GetImageFromRenderGraph(outputImageNode->GetImage());
        if (outputImage && Gfx::IsDepthStencilFormat(outputImage->GetDescription().format))
        {
            return &outputImageNode->GetImage();
        }
    }

    return nullptr;
}
} // namespace Rendering::FrameGraph
