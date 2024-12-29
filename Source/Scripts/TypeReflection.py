import tree_sitter
import tree_sitter_cpp as tscpp
from tree_sitter import Language, Parser, Tree, Node, TreeCursor
from typing import Generator

type_list = [

]

code = '''
class GameObject
{
public:
    [[Reflect]] bool isPrototype;
    [[Reflect]] glm::vec3 position = glm::vec3(0);
    [[Reflect]] glm::vec3 scale = glm::vec3(1, 1, 1);
    [[Reflect]] glm::quat rotation = glm::quat(1, 0, 0, 0);
};
'''

code = open("D:/WeilanEngine/Source/Engine/Rendering/RenderPipeline/RenderPipeline.hpp").read()

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
    field_name = ""
    type_name = ""

    # 0: function, 1: field
    type = -1

    # attribute_name
    attributeNameNode = n.child(1)
    attributeNameNode = attributeNameNode.child_by_field_name("name")
    attribute_name = code[attributeNameNode.start_byte:attributeNameNode.end_byte]

    parent_node : Node = n.parent
    if parent_node.type == "field_declaration":
        field_declaration_node = parent_node
    
        declarator_node = field_declaration_node.child_by_field_name("declarator")
        type_node = field_declaration_node.child_by_field_name("type")
        type_name = code[type_node.start_byte:type_node.end_byte]

        # process as a function declaration
        if declarator_node.type == "function_declarator":
            typeNode = field_declaration_node.child_by_field_name("type")
            # return_type = code[typeNode.start_byte:typeNode.end_byte]

            func_declaratorNode_node = declarator_node.child_by_field_name("declarator")
            field_name = code[func_declaratorNode_node.start_byte:func_declaratorNode_node.end_byte]
            type = 0

        # process as a member variable
        elif declarator_node.type == "field_identifier":
            field_name = code[declarator_node.start_byte:declarator_node.end_byte]
            type = 1

    # process as a function defination
    if parent_node.type == "function_definition":
        function_defination_node : Node = parent_node
        children = traverse_tree(function_defination_node.walk())
        for c in children:
            if c.type == "function_declarator":
                declarator_node = c.child_by_field_name("declarator")
                field_name = code[declarator_node.start_byte:declarator_node.end_byte]
                type = 0
                break
    
    return attribute_name, type, type_name, field_name

current_class_nodes = dict()
current_class_node = None
for n in nodes:
    if n.type == "class_specifier":
        current_class_nodes[n] = list()
        current_class_node = n
        class_name_node = n.child_by_field_name("name")
    if n.type == "attribute_declaration":
        attribute_name, type, type_name, field_name = function_parameter_from_attribute_node(n)
        current_class_nodes[current_class_node].append(tuple([attribute_name, type, type_name, field_name]))

output = ''
output += "#pragma once\n"
output += "/****** GENERATED *******/\n"
output += "#include \"Libs/RTTI.hpp\"\n"
output += "static void RegisterSerializedObjects()\n"
output += "{\n"

for k, parsed in current_class_nodes.items():
    class_name_node = k.child_by_field_name("name")
    class_name = code[class_name_node.start_byte:class_name_node.end_byte]
    for n in parsed:
        type = n[1]
        attribute_name = n[0]
        if (type == 1 and attribute_name == "Reflect"):
            class_name = class_name
            field_name = n[3]
            output += "    REGISTER_TYPE_REFLECTION_MEMBER_VARIABLE(" + class_name + ", " + field_name + ");\n"

output += "}\n"

print(output)
