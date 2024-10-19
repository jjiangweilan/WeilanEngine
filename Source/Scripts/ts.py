import tree_sitter
import tree_sitter_cpp as tscpp
from tree_sitter import Language, Parser, Tree, Node, TreeCursor
from typing import Generator

code = '''
#pragma once

#include "Component.hpp"
#include "Core/Math/Geometry.hpp"
#include <glm/glm.hpp>

namespace Rendering::FrameGraph
{
class Graph;
}
class Camera : public Component
{
    DECLARE_OBJECT();

public:
    Camera();
    Camera(GameObject* gameObject);
    ~Camera() override {};
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    glm::mat4 GetViewMatrix() const;
    const glm::mat4& GetProjectionMatrix() const;
    glm::vec3 ScreenUVToViewSpace(glm::vec2 screenUV);
    glm::vec3 ScreenUVToWorldPos(glm::vec2 screenUV);
    glm::vec3 GetForward();
    Ray ScreenUVToWorldSpaceRay(glm::vec2 screenUV);
    void SetProjectionMatrix(float fovy, float aspect, float zNear, float zFar);
    void SetProjectionMatrix(const glm::mat4& proj);

    [[LuaBinding]]
    void SetDiffuseEnv(Texture* cubemap);
    [[LuaBinding]]
    void SetSpecularEnv(Texture* cubemap);
    [[LuaBinding]]
    const SRef<Texture>& GetDiffuseEnv()
    {
        return diffuseEnv;
    }
    [[LuaBinding]]
    const SRef<Texture> GetDiffuseEnvv()
    {
        return diffuseEnv;
    }

    const SRef<Texture>& GetSpecularEnv()
    {
        return specularEnv;
    }

    void SetFrameGraph(Rendering::FrameGraph::Graph* graph);
    Rendering::FrameGraph::Graph* GetFrameGraph() const
    {
        return frameGraph;
    }

    static RefPtr<Camera> mainCamera;

    float GetProjectionRight();
    float GetProjectionTop();
    float GetNear();
    float GetFar();

    void DrawGizmos();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void OnDrawGizmos() override;

private:
    SRef<Texture> diffuseEnv = nullptr;
    SRef<Texture> specularEnv = nullptr;
    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;
    Rendering::FrameGraph::Graph* frameGraph = nullptr;
    float near;
    float far;
    float fov;
    float aspect;
};'''

CPP_LANGUAGE = Language(tscpp.language())
parser = Parser(CPP_LANGUAGE)
tree = parser.parse(bytes(code, "utf8"))

def traverse_tree_internal(cursor: TreeCursor) -> Generator[Node, None, None]:
    visited_children = False
    while True:
        if not visited_children:
            yield cursor.node
            if not cursor.goto_first_child():
                visited_children = True
        elif cursor.goto_next_sibling():
            visited_children = False
        elif not cursor.goto_parent():
            break

def traverse_tree(cursor: TreeCursor):
    return map(lambda node: node, traverse_tree_internal(cursor))

nodes = traverse_tree(tree.root_node.walk())

def function_parameter_from_attribute_node(attribute_node):
    n = attribute_node
    if (n.type != "attribute_declaration"):
        return

    attribute_name = ""
    function_name = ""

    # attribute_name
    attributeNameNode = n.child(1)
    attributeNameNode = attributeNameNode.child_by_field_name("name")
    attribute_name = code[attributeNameNode.start_byte:attributeNameNode.end_byte]

    parent_node : Node = n.parent
    if parent_node.type == "field_declaration":
        field_declaration_node = parent_node
    
        declarator_node : Node = field_declaration_node.child_by_field_name("declarator")

        # process as a function declaration
        if declarator_node.type == "function_declarator":
            typeNode = field_declaration_node.child_by_field_name("type")
            # return_type = code[typeNode.start_byte:typeNode.end_byte]

            func_declaratorNode_node = declarator_node.child_by_field_name("declarator")
            function_name = code[func_declaratorNode_node.start_byte:func_declaratorNode_node.end_byte]

        # process as a member variable
        # elif declarator_node.type == "field_identifier":
            #TODO

    # process as a function defination
    if parent_node.type == "function_definition":
        function_defination_node : Node = parent_node
        children = traverse_tree(function_defination_node.walk())
        for c in children:
            if c.type == "function_declarator":
                declarator_node = c.child_by_field_name("declarator")
                function_name = code[declarator_node.start_byte:declarator_node.end_byte]
                break
    
    return attribute_name, function_name
    
for n in nodes:
    if (n.type == "attribute_declaration"):
        attribute_name, function_name = function_parameter_from_attribute_node(n)
        print(attribute_name, function_name)