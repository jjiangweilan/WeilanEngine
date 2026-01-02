import os
import glob
import tree_sitter
import tree_sitter_cpp as tscpp
from tree_sitter import Language, Parser, Node

# Initialize Tree-sitter
CPP_LANGUAGE = Language(tscpp.language())
parser = Parser(CPP_LANGUAGE)

def get_node_text(node, code_bytes):
    return code_bytes[node.start_byte:node.end_byte].decode('utf-8')

def find_attribute(node, code_bytes, target_attr):
    """
    Checks if a node has a specific attribute (e.g., [[LuaClass]]).
    Returns the attribute node if found, None otherwise.
    """
    # In tree-sitter-cpp, attributes often appear as siblings or children depending on context
    # We look for 'attribute_declaration' nodes usually preceding the declaration
    # or inside the declaration node's children.
    
    # Helper to check a specific attribute node
    def is_target_attr(attr_node):
        if attr_node.type == 'attribute_declaration':
            # attribute_declaration -> attribute -> identifier/call_expression
            # We iterate over children to find the 'attribute' part
            for child in attr_node.children:
                if child.type == 'attribute':
                    name_node = child.child_by_field_name('name')
                    if name_node and get_node_text(name_node, code_bytes) == target_attr:
                        return True
        return False

    # Strategy 1: Check children (common for function/field definitions)
    for child in node.children:
        if is_target_attr(child):
            return True
            
    # Strategy 2: Check previous sibling (sometimes attributes are parsed as siblings)
    prev = node.prev_sibling
    while prev:
        if is_target_attr(prev):
            return True
        # Skip comments or whitespace if they appear as nodes (depends on grammar)
        if prev.type not in ['comment', 'attribute_declaration']:
            break
        prev = prev.prev_sibling

    return False

def get_full_qualified_name(node, code_bytes):
    """
    Recursively builds the namespace::ClassName string.
    """
    name_node = node.child_by_field_name('name')
    if not name_node:
        return ""
    name = get_node_text(name_node, code_bytes)
    
    parent = node.parent
    while parent:
        if parent.type == 'namespace_definition':
            ns_name_node = parent.child_by_field_name('name')
            if ns_name_node:
                ns = get_node_text(ns_name_node, code_bytes)
                name = f"{ns}::{name}"
        parent = parent.parent
    return name

def process_file(file_path):
    with open(file_path, 'rb') as f:
        code_bytes = f.read()
    
    tree = parser.parse(code_bytes)
    cursor = tree.walk()
    
    classes_to_bind = []

    # Simple recursive traversal
    def traverse(node):
        if node.type in ['class_specifier', 'struct_specifier']:
            if find_attribute(node, code_bytes, 'LuaClass'):
                class_info = {
                    'name': get_full_qualified_name(node, code_bytes),
                    'methods': [],
                    'properties': [],
                    'static_methods': []
                }
                
                # Traverse class body for members
                body = node.child_by_field_name('body')
                if body:
                    for member in body.children:
                        
                        # Determine if this node represents a function or a property
                        is_method = False
                        func_declarator = None
                        
                        if member.type == 'function_definition':
                            is_method = True
                            func_declarator = member.child_by_field_name('declarator')
                        elif member.type in ['field_declaration', 'declaration']:
                            # Check if it has a function_declarator child
                            # In field_declaration, the declarator is usually a child, but not always named 'declarator' 
                            # (e.g. if there are storage specifiers).
                            # We iterate to find function_declarator
                            for child in member.children:
                                if child.type == 'function_declarator':
                                    is_method = True
                                    func_declarator = child
                                    break
                            
                            # Fallback: check 'declarator' field if no direct child found (unlikely for field_declaration but good for safety)
                            if not is_method:
                                d = member.child_by_field_name('declarator')
                                if d and d.type == 'function_declarator':
                                    is_method = True
                                    func_declarator = d

                        if is_method:
                            if find_attribute(member, code_bytes, 'LuaFn'):
                                if not func_declarator: continue

                                # Extract name from function_declarator
                                # function_declarator -> declarator (identifier)
                                # but sometimes nested (pointer, reference, etc, though less common for method names themselves)
                                d = func_declarator.child_by_field_name('declarator')
                                func_name = get_node_text(d, code_bytes)
                                
                                # Check if static
                                is_static = False
                                for child in member.children:
                                    if child.type == 'storage_class_specifier' and get_node_text(child, code_bytes) == 'static':
                                        is_static = True
                                        break
                                
                                if func_name:
                                    if is_static:
                                        class_info['static_methods'].append(func_name)
                                    else:
                                        class_info['methods'].append(func_name)

                        # Properties (Member variables)
                        elif member.type == 'field_declaration':
                            if find_attribute(member, code_bytes, 'LuaProp'):
                                # It's a property if it wasn't a method
                                declarator = member.child_by_field_name('declarator')
                                # Sometimes it's a field_identifier directly or inside
                                if not declarator:
                                    # Try finding field_identifier in children
                                    for child in member.children:
                                        if child.type == 'field_identifier':
                                            declarator = child
                                            break
                                
                                if declarator:
                                    prop_name = get_node_text(declarator, code_bytes)
                                    class_info['properties'].append(prop_name)

                classes_to_bind.append(class_info)
        
        for child in node.children:
            traverse(child)

    traverse(tree.root_node)
    return classes_to_bind

def generate_bindings(source_dir, output_file):
    all_bindings = []
    
    # scan for .hpp files
    files = glob.glob(os.path.join(source_dir, '**/*.hpp'), recursive=True)
    
    for file_path in files:
        # relative path for #include
        rel_path = os.path.relpath(file_path, source_dir).replace('\\', '/')
        classes = process_file(file_path)
        if classes:
            all_bindings.append({'file': rel_path, 'classes': classes})

    # Generate content
    out = []
    out.append("// GENERATED FILE - DO NOT EDIT")
    out.append('#include "LuaBindings.hpp"')
    out.append('#include "LuaBindings_Private.hpp"') # Ensure we have access to helpers
    out.append("")
    
    # Includes
    for entry in all_bindings:
        out.append(f'#include "{entry["file"]}"')
    
    out.append("")
    out.append("void BindGeneratedClasses(lua_State* L)")
    out.append("{")
    
    for entry in all_bindings:
        for cls in entry['classes']:
            class_name = cls['name']
            # Assume Lua name matches Class name for now (stripped of namespace)
            lua_name = class_name.split('::')[-1]
            
            out.append(f"    LuaBinder<{class_name}> binder_{lua_name}(L);")
            out.append(f"    binder_{lua_name}.Begin(\"{lua_name}\")")
            
            for m in cls['methods']:
                out.append(f"        .BindMemFn(\"{m}\", &{class_name}::{m})")
            
            for m in cls['static_methods']:
                out.append(f"        .BindStaticFn(\"{m}\", &{class_name}::{m})")
                
            for p in cls['properties']:
                out.append(f"        .BindProperty(\"{p}\", &{class_name}::{p})")
                
            out.append("        .End();")
            out.append("")
            
    out.append("}")
    
    with open(output_file, 'w') as f:
        f.write("\n".join(out))
    
    print(f"Generated bindings for {len(all_bindings)} files to {output_file}")

if __name__ == "__main__":
    # Adjust paths as needed
    source_root = "Source"
    output_path = "Source/Engine/CodeGen/LuaBindings_Generated.cpp"
    generate_bindings(source_root, output_path)
