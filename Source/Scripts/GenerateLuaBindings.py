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

def get_type_name(node, code_bytes, pointer_depth=0, is_reference=False):
    """
    Extracts a simplified type name from a type node.
    """
    if not node: return "void"
    text = get_node_text(node, code_bytes)
    # Simplify common C++ types for Lua
    text = text.replace("const", "").replace("&", "").replace("*", "").strip()
    # Add back pointer/reference modifiers
    text = text + "*" * pointer_depth
    if is_reference:
        text = text + "&"
    return text

def unwrap_declarator(declarator):
    """
    Unwraps pointer_declarator and reference_declarator nodes to get the inner declarator.
    Returns (inner_declarator, pointer_depth, is_reference).
    """
    pointer_depth = 0
    is_reference = False
    
    while declarator:
        if declarator.type == 'pointer_declarator':
            pointer_depth += 1
            declarator = declarator.child_by_field_name('declarator')
        elif declarator.type == 'reference_declarator':
            is_reference = True
            declarator = declarator.child_by_field_name('declarator')
        else:
            break
    
    return declarator, pointer_depth, is_reference

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
                        return child
        return None

    # Strategy 1: Check children (common for function/field definitions)
    for child in node.children:
        res = is_target_attr(child)
        if res: return res
            
    # Strategy 2: Check previous sibling (sometimes attributes are parsed as siblings)
    prev = node.prev_sibling
    while prev:
        res = is_target_attr(prev)
        if res: return res
        # Skip comments or whitespace if they appear as nodes (depends on grammar)
        if prev.type not in ['comment', 'attribute_declaration']:
            break
        prev = prev.prev_sibling

    return None

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
    classes_to_bind = []
    enums_to_bind = []

    # Simple recursive traversal
    def traverse(node):
        if node.type in ['class_specifier', 'struct_specifier']:
            if find_attribute(node, code_bytes, 'LuaClass'):
                class_info = {
                    'name': get_full_qualified_name(node, code_bytes),
                    'methods': [],
                    'properties': [],
                    'static_methods': [],
                    'raw_methods': []
                }
                
                # Traverse class body for members
                body = node.child_by_field_name('body')
                if body:
                    for member in body.children:
                        
                        # Determine if this node represents a function or a property
                        is_method = False
                        func_declarator = None
                        ret_type_node = None
                        ret_pointer_depth = 0
                        ret_is_reference = False
                        
                        if member.type == 'function_definition':
                            is_method = True
                            raw_declarator = member.child_by_field_name('declarator')
                            func_declarator, ret_pointer_depth, ret_is_reference = unwrap_declarator(raw_declarator)
                            ret_type_node = member.child_by_field_name('type')
                        elif member.type in ['field_declaration', 'declaration']:
                            ret_type_node = member.child_by_field_name('type')
                            # Check if it has a function_declarator child
                            for child in member.children:
                                if child.type == 'function_declarator':
                                    is_method = True
                                    func_declarator = child
                                    break
                                elif child.type in ['pointer_declarator', 'reference_declarator']:
                                    # Unwrap to check if it contains a function_declarator
                                    unwrapped, pd, ir = unwrap_declarator(child)
                                    if unwrapped and unwrapped.type == 'function_declarator':
                                        is_method = True
                                        func_declarator = unwrapped
                                        ret_pointer_depth = pd
                                        ret_is_reference = ir
                                        break
                            
                            # Fallback
                            if not is_method:
                                d = member.child_by_field_name('declarator')
                                if d:
                                    unwrapped, pd, ir = unwrap_declarator(d)
                                    if unwrapped and unwrapped.type == 'function_declarator':
                                        is_method = True
                                        func_declarator = unwrapped
                                        ret_pointer_depth = pd
                                        ret_is_reference = ir

                        if is_method:
                            lua_fn_attr = find_attribute(member, code_bytes, 'LuaFn')
                            lua_named_fn_attr = find_attribute(member, code_bytes, 'LuaNamedFn')
                            lua_raw_fn_attr = find_attribute(member, code_bytes, 'LuaRawFn')
                            
                            if lua_fn_attr or lua_named_fn_attr:
                                if not func_declarator: continue

                                # Extract name from function_declarator
                                d = func_declarator.child_by_field_name('declarator')
                                func_name = get_node_text(d, code_bytes)
                                
                                # Check if static
                                is_static = False
                                for child in member.children:
                                    if child.type == 'storage_class_specifier' and get_node_text(child, code_bytes) == 'static':
                                        is_static = True
                                        break
                                
                                # Extract return type (including pointer/reference modifiers)
                                ret_type = get_type_name(ret_type_node, code_bytes, ret_pointer_depth, ret_is_reference)

                                # Extract parameters
                                params = []
                                parameters_node = func_declarator.child_by_field_name('parameters')
                                if parameters_node:
                                    for param in parameters_node.children:
                                        if param.type == 'parameter_declaration':
                                            p_type_node = param.child_by_field_name('type')
                                            p_name_node = param.child_by_field_name('declarator')
                                            
                                            p_type = get_type_name(p_type_node, code_bytes)
                                            p_name = get_node_text(p_name_node, code_bytes) if p_name_node else "arg"
                                            params.append(f"{p_type} {p_name}")

                                sig = f"// {ret_type}({', '.join(params)})"
                                
                                bind_name = func_name
                                if lua_named_fn_attr:
                                    # Extract binding name from arguments
                                    args_node = lua_named_fn_attr.child_by_field_name('arguments')
                                    if not args_node:
                                        for child in lua_named_fn_attr.children:
                                            if child.type == 'argument_list':
                                                args_node = child
                                                break
                                    if args_node:
                                        for arg in args_node.children:
                                            if arg.type == 'string_literal':
                                                bind_name = get_node_text(arg, code_bytes).strip('"')
                                                break

                                if func_name:
                                    if is_static:
                                        class_info['static_methods'].append({'name': func_name, 'bind_name': bind_name, 'sig': sig})
                                    else:
                                        class_info['methods'].append({'name': func_name, 'bind_name': bind_name, 'sig': sig})
                            
                            elif lua_raw_fn_attr:
                                if not func_declarator: continue

                                # Extract name from function_declarator
                                d = func_declarator.child_by_field_name('declarator')
                                func_name = get_node_text(d, code_bytes)
                                
                                bind_name = func_name
                                
                                # Check arguments in attribute for custom name
                                args_node = lua_raw_fn_attr.child_by_field_name('arguments')
                                if not args_node:
                                    for child in lua_raw_fn_attr.children:
                                        if child.type == 'argument_list':
                                            args_node = child
                                            break

                                if args_node:
                                    for arg in args_node.children:
                                        if arg.type == 'string_literal':
                                            bind_name = get_node_text(arg, code_bytes).strip('"')
                                            break
                                
                                if func_name:
                                    class_info['raw_methods'].append({'name': func_name, 'bind_name': bind_name})

                        # Properties (Member variables)
                        elif member.type == 'field_declaration':
                            if find_attribute(member, code_bytes, 'LuaProp'):
                                # It's a property if it wasn't a method
                                declarator = member.child_by_field_name('declarator')
                                prop_type_node = member.child_by_field_name('type')
                                
                                # Sometimes it's a field_identifier directly or inside
                                if not declarator:
                                    # Try finding field_identifier in children
                                    for child in member.children:
                                        if child.type == 'field_identifier':
                                            declarator = child
                                            break
                                
                                if declarator:
                                    prop_name = get_node_text(declarator, code_bytes)
                                    prop_type = get_type_name(prop_type_node, code_bytes)
                                    class_info['properties'].append({'name': prop_name, 'sig': f"// {prop_type}"})

                classes_to_bind.append(class_info)

        elif node.type == 'enum_specifier':
            if find_attribute(node, code_bytes, 'LuaEnum'):
                enum_info = {
                    'name': get_full_qualified_name(node, code_bytes),
                    'values': []
                }
                body = node.child_by_field_name('body')
                if body:
                    for child in body.children:
                        if child.type == 'enumerator':
                            name_node = child.child_by_field_name('name')
                            if name_node:
                                enum_info['values'].append(get_node_text(name_node, code_bytes))
                enums_to_bind.append(enum_info)
        
        elif node.type == 'declaration':
            # Handle case where attribute is in the middle: enum class [[LuaEnum]] MyEnum { ... };
            # Tree-sitter parses this as a declaration with enum_specifier, attribute_declaration, and init_declarator
            enum_spec = None
            has_lua_enum = False
            enum_name = None
            
            for child in node.children:
                if child.type == 'enum_specifier':
                    enum_spec = child
                elif child.type == 'attribute_declaration':
                    # Check if it's LuaEnum
                    for attr in child.children:
                        if attr.type == 'attribute':
                            name_node = attr.child_by_field_name('name')
                            if name_node and get_node_text(name_node, code_bytes) == 'LuaEnum':
                                has_lua_enum = True
                elif child.type == 'init_declarator':
                    name_node = child.child_by_field_name('declarator')
                    if name_node:
                        enum_name = get_node_text(name_node, code_bytes)

            if has_lua_enum and enum_spec:
                # We need to find the enumerators. They might be in the enum_spec or in the init_declarator
                # In the case of `enum class [[LuaEnum]] TestEnum { Value1, Value2 };`
                # Tree-sitter might put the body in the init_declarator's initializer_list if it's confused
                # but usually enums have a body.
                body = enum_spec.child_by_field_name('body')
                if not body:
                    # Check init_declarator for initializer_list (confused parser)
                    for child in node.children:
                        if child.type == 'init_declarator':
                            for grandchild in child.children:
                                if grandchild.type == 'initializer_list':
                                    body = grandchild
                                    break
                
                if body and enum_name:
                    enum_info = {
                        'name': enum_name, # TODO: fully qualified name?
                        'values': []
                    }
                    for child in body.children:
                        # For initializer_list, children might be identifiers or assignment_expressions
                        # For enum_specifier body, children are enumerators
                        if child.type == 'enumerator':
                            v_name_node = child.child_by_field_name('name')
                            if v_name_node:
                                enum_info['values'].append(get_node_text(v_name_node, code_bytes))
                        elif child.type == 'identifier':
                            enum_info['values'].append(get_node_text(child, code_bytes))
                        elif child.type == 'assignment_expression':
                            # Get the left side of the assignment
                            v_name_node = child.child_by_field_name('left')
                            if not v_name_node:
                                # Sometimes it's just the first identifier
                                for grandchild in child.children:
                                    if grandchild.type == 'identifier':
                                        v_name_node = grandchild
                                        break
                            if v_name_node:
                                enum_info['values'].append(get_node_text(v_name_node, code_bytes))
                    enums_to_bind.append(enum_info)

        for child in node.children:
            traverse(child)

    traverse(tree.root_node)
    return {'classes': classes_to_bind, 'enums': enums_to_bind}

def generate_bindings(source_dir, output_file):
    all_bindings = []
    
    # scan for .hpp files
    files = glob.glob(os.path.join(source_dir, '**/*.hpp'), recursive=True)
    
    for file_path in files:
        # relative path for #include
        rel_path = os.path.relpath(file_path, source_dir).replace('\\', '/')
        data = process_file(file_path)
        if data['classes'] or data['enums']:
            all_bindings.append({'file': rel_path, 'classes': data['classes'], 'enums': data['enums']})

    # Generate content
    out = []
    out.append("// GENERATED FILE - DO NOT EDIT")
    out.append('#include "Engine/Runtime/System/ScriptingBackend/LuaBindings.hpp"')
    out.append('#include "Engine/Runtime/System/ScriptingBackend/LuaBindings_Private.hpp"')
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
                out.append(f"        .BindMemFn(\"{m['bind_name']}\", &{class_name}::{m['name']}) {m['sig']}")
            
            for m in cls['static_methods']:
                out.append(f"        .BindStaticFn(\"{m['bind_name']}\", &{class_name}::{m['name']}) {m['sig']}")

            for m in cls['raw_methods']:
                out.append(f"        .BindFn(\"{m['bind_name']}\", &{class_name}::{m['name']})")
                
            for p in cls['properties']:
                out.append(f"        .BindProperty(\"{p['name']}\", &{class_name}::{p['name']}) {p['sig']}")
                
            out.append("        .End();")
            out.append("")

        for enum in entry['enums']:
            enum_name = enum['name']
            lua_name = enum_name.split('::')[-1]
            out.append(f"    // Bind Enum {enum_name}")
            out.append(f"    lua_newtable(L);")
            for val in enum['values']:
                out.append(f"    lua_pushinteger(L, static_cast<int>({enum_name}::{val}));")
                out.append(f"    lua_setfield(L, -2, \"{val}\");")
            
            # Register to wl table (assuming wl table is at -2 relative to stack top BEFORE we pushed the enum table)
            # Stack state: [..., wl, enum_table]
            # We want wl[lua_name] = enum_table.
            # lua_setfield(L, -2, "name") will set wl["name"] = enum_table and pop enum_table.
            
            out.append(f"    lua_setfield(L, -2, \"{lua_name}\");")
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
