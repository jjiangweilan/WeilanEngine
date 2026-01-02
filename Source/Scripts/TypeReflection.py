import os
import glob
import tree_sitter
import tree_sitter_cpp as tscpp
from tree_sitter import Language, Parser

# Initialize Tree-sitter
CPP_LANGUAGE = Language(tscpp.language())
parser = Parser(CPP_LANGUAGE)

def get_node_text(node, code_bytes):
    return code_bytes[node.start_byte:node.end_byte].decode('utf-8')

def find_attribute(node, code_bytes, target_attr):
    """
    Checks if a node has a specific attribute (e.g., [[Reflectable]]).
    """
    # Check children (e.g. inside class_specifier, function_declarator)
    for child in node.children:
        if child.type == 'attribute_declaration':
            for subchild in child.children:
                if subchild.type == 'attribute':
                    name = subchild.child_by_field_name('name')
                    if name and get_node_text(name, code_bytes) == target_attr:
                        return True
    
    # Check attributed_declarator (common for fields)
    if node.type == 'attributed_declarator':
        for child in node.children:
             if child.type == 'attribute_declaration':
                for subchild in child.children:
                    if subchild.type == 'attribute':
                        name = subchild.child_by_field_name('name')
                        if name and get_node_text(name, code_bytes) == target_attr:
                            return True
    
    return False

def get_namespace_list(node, code_bytes):
    """
    Returns a list of namespaces ['Engine', 'Runtime'] for the given node.
    """
    namespaces = []
    parent = node.parent
    while parent:
        if parent.type == 'namespace_definition':
            name_node = parent.child_by_field_name('name')
            if name_node:
                namespaces.append(get_node_text(name_node, code_bytes))
        parent = parent.parent
    return list(reversed(namespaces))

def process_file(file_path):
    with open(file_path, 'rb') as f:
        code_bytes = f.read()
    
    tree = parser.parse(code_bytes)
    
    classes_found = []

    def traverse(node):
        if node.type in ['class_specifier', 'struct_specifier']:
            if find_attribute(node, code_bytes, 'TrClass'):
                class_name_node = node.child_by_field_name('name')
                if not class_name_node:
                    return # Anonymous class? 

                class_name = get_node_text(class_name_node, code_bytes)
                namespaces = get_namespace_list(node, code_bytes)
                
                class_info = {
                    'name': class_name,
                    'namespaces': namespaces,
                    'properties': [],
                    'functions': []
                }

                # Traverse body for members
                body = node.child_by_field_name('body')
                if body:
                    for member in body.children:
                        # Fields (Properties)
                        if member.type == 'field_declaration':
                            # Check for [[Property]]
                            # Case 1: Direct attribute on field_declaration (less common in C++ for fields, but possible)
                            # Case 2: attributed_declarator inside
                            
                            prop_name = None
                            is_property = False
                            
                            # Inspect children for declarators
                            for child in member.children:
                                if child.type == 'attributed_declarator':
                                    if find_attribute(child, code_bytes, 'TrProp'):
                                        # Extract name from the declarator inside
                                        # attributed_declarator usually contains a field_identifier or type_identifier
                                        # It might wrap another declarator
                                        for sub in child.children:
                                            if sub.type == 'field_identifier':
                                                prop_name = get_node_text(sub, code_bytes)
                                                is_property = True
                                                break
                                elif child.type == 'field_identifier':
                                    # Maybe attribute was elsewhere?
                                    pass
                            
                            if is_property and prop_name:
                                class_info['properties'].append(prop_name)
                                continue

                            # Functions (Methods) marked with [[Fn]] inside field_declaration context
                            # Sometimes methods are parsed as field_declaration if they look like one?
                            # But usually they are function_definition or declaration.
                            # However, in the AST dump, we saw 'field_declaration' containing 'function_declarator'.
                            
                            func_declarator = None
                            for child in member.children:
                                if child.type == 'function_declarator':
                                    func_declarator = child
                                    break
                            
                            if func_declarator:
                                if find_attribute(func_declarator, code_bytes, 'TrFn'):
                                    # Extract name
                                    # function_declarator -> field_identifier or identifier
                                    # Need to be careful about finding the name node.
                                    # In the AST dump: 
                                    # function_declarator: 'GetComponent(...)'
                                    #   field_identifier: 'GetComponent'
                                    name_node = None
                                    for sub in func_declarator.children:
                                        if sub.type in ['field_identifier', 'identifier']:
                                            name_node = sub
                                            break
                                    
                                    if name_node:
                                        func_name = get_node_text(name_node, code_bytes)
                                        class_info['functions'].append(func_name)

                        # Function Definitions (Inline implementation)
                        elif member.type == 'function_definition':
                            func_declarator = member.child_by_field_name('declarator')
                            if func_declarator and find_attribute(func_declarator, code_bytes, 'Fn'):
                                name_node = func_declarator.child_by_field_name('declarator')
                                # Note: 'declarator' field of function_definition is a function_declarator
                                # inside function_declarator, the name is usually also 'declarator' or just a child identifier
                                
                                # tree-sitter-cpp structure varies.
                                # specific check:
                                if name_node:
                                    # if name_node is the identifier
                                    func_name = get_node_text(name_node, code_bytes)
                                    class_info['functions'].append(func_name)
                                else:
                                    # iterate children of function_declarator
                                    for sub in func_declarator.children:
                                        if sub.type in ['identifier', 'field_identifier']:
                                            class_info['functions'].append(get_node_text(sub, code_bytes))
                                            break

                        # Regular Declarations (Methods without body in class, or just ; at end)
                        elif member.type == 'declaration':
                             func_declarator = member.child_by_field_name('declarator')
                             if func_declarator and func_declarator.type == 'function_declarator':
                                 if find_attribute(func_declarator, code_bytes, 'Fn'):
                                     # extract name
                                     for sub in func_declarator.children:
                                         if sub.type in ['identifier', 'field_identifier', 'destructor_name']:
                                             # Destructor shouldn't be reflected usually, but if tagged...
                                             class_info['functions'].append(get_node_text(sub, code_bytes))
                                             break


                classes_found.append(class_info)

        for child in node.children:
            traverse(child)

    traverse(tree.root_node)
    return classes_found

def generate_reflection_code(source_dir, output_file):
    all_classes = []
    
    # Scan all .hpp files
    files = glob.glob(os.path.join(source_dir, '**/*.hpp'), recursive=True)
    
    for file_path in files:
        classes = process_file(file_path)
        if classes:
            # Calculate relative include path
            rel_path = os.path.relpath(file_path, os.path.dirname(output_file)).replace('\\', '/')
            all_classes.append({'include': rel_path, 'classes': classes})

    out = []
    out.append("// GENERATED FILE - DO NOT EDIT")
    out.append("// This file registers reflection data for classes marked with [[Reflectable]]")
    out.append("")
    out.append('#include "Engine/Library/TypeReflection.hpp"')
    out.append("")
    
    # Includes
    for entry in all_classes:
        out.append(f'#include "{entry["include"]}"')
    
    out.append("")
    
    for entry in all_classes:
        for cls in entry['classes']:
            name = cls['name']
            namespaces = cls['namespaces']
            
            # Open namespaces
            for ns in namespaces:
                out.append(f"namespace {ns} {{")
            
            indent = "" # Indent is handled by namespace wrapping implicitly or we can be explicit
            
            # Member Variables
            if cls['properties']:
                out.append(f"TYPE_REFLECTION_MEMBER_VARIABLES({name},")
                for i, prop in enumerate(cls['properties']):
                    comma = "," if i < len(cls['properties']) - 1 else ""
                    out.append(f"    TYPE_REFLECTION_MEM({name}, {prop}){comma}")
                out.append(");")
                out.append("")

            # Member Functions
            if cls['functions']:
                out.append(f"TYPE_REFLECTION_MEMBER_FUNCTIONS({name},")
                for i, func in enumerate(cls['functions']):
                    comma = "," if i < len(cls['functions']) - 1 else ""
                    out.append(f"    TYPE_REFLECTION_FUNC({name}, {func}){comma}")
                out.append(");")
                out.append("")

            # Close namespaces
            for ns in reversed(namespaces):
                out.append(f"}} // namespace {ns}")
            out.append("")

    # Ensure directory exists
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'w') as f:
        f.write("\n".join(out))
    
    print(f"Generated reflection code for {len(all_classes)} files to {output_file}")

if __name__ == "__main__":
    source_root = "Source"
    # Output to a sensible location. 
    # Since GameObject.cpp is in Source/Engine/Runtime/Object/GameObject/,
    # Putting this in Source/Engine/CodeGen/ seems appropriate.
    output_path = "Source/Engine/CodeGen/TypeReflection_Generated.cpp"
    generate_reflection_code(source_root, output_path)
