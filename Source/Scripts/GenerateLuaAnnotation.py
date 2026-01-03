import re
import os

# Configuration
OUTPUT_FILE = os.path.join("Source", "Lua", "weilan_api.lua")
INPUT_FILES = [
    os.path.join("Source", "Engine", "CodeGen", "LuaBindings_Generated.cpp"),
    os.path.join("Source", "Engine", "Runtime", "System", "ScriptingBackend", "LuaBindings.cpp")
]

def strip_comments_but_keep_lines(content):
    # We want to keep the single line comments at the end of lines if they contain type info
    # But we want to strip block comments or other noise?
    # Actually, the regexes below will handle finding the comment at the end of the line.
    # So we don't need to strip everything blindly.
    return content

def cpp_type_to_lua(cpp_type):
    cpp_type = cpp_type.strip()
    if cpp_type == "void": return None
    if cpp_type in ["float", "double", "int", "unsigned int", "size_t", "long", "short", "uint8_t", "int32_t", "uint32_t"]: return "number"
    if cpp_type == "bool": return "boolean"
    if cpp_type in ["std::string", "const char*", "string", "std::string &", "const std::string &"]: return "string"
    
    # Check for vectors
    if cpp_type in ["glm::vec2", "float2", "const float2&", "const glm::vec2&"]: return "wl.Float2"
    if cpp_type in ["glm::vec3", "float3", "const float3&", "const glm::vec3&"]: return "wl.Float3"
    if cpp_type in ["glm::vec4", "float4", "const float4&", "const glm::vec4&", "wl.vec4&"]: return "wl.Float4"
    
    # Generic object pointer handling
    # ObjPtr<ClassName> -> wl.ClassName
    match = re.match(r'ObjPtr<(\w+)>', cpp_type)
    if match:
        return f"wl.{match.group(1)}"

    # Default to assuming it's a class we bound or 'any'
    # Remove namespace for simplicity if it looks like Engine::Object
    if "::" in cpp_type:
        parts = cpp_type.split("::")
        return f"wl.{parts[-1]}"
    
    # If it's a simple name like 'GameObject', assume wl.GameObject
    # But skip 'any' or other primitives
    if cpp_type and cpp_type[0].isupper():
        return f"wl.{cpp_type}"

    return "any"

def parse_signature(sig_comment):
    """
    Parses '// RetType(ParamType p1, ParamType2 p2)'
    Returns {'ret': 'lua_type', 'params': [{'name': 'p1', 'type': 'lua_type'}, ...]}
    """
    if not sig_comment:
        return {'ret': None, 'params': []}
    
    # Remove '// '
    content = sig_comment.replace('//', '').strip()
    
    # Check if it's a property type (no parenthesis)
    if '(' not in content:
        return {'type': cpp_type_to_lua(content)}

    # Function signature
    # Split into RetType and Params part
    match = re.match(r'(.+?)\((.*)\)', content)
    if not match:
        return {'ret': None, 'params': []}
    
    ret_cpp = match.group(1).strip()
    params_content = match.group(2).strip()
    
    ret_lua = cpp_type_to_lua(ret_cpp)
    
    params = []
    if params_content:
        # Split by comma, but careful about templates? For now assume simple types
        # A robust split would track brackets.
        param_list = [p.strip() for p in params_content.split(',')]
        for p in param_list:
            # Last word is name, rest is type
            parts = p.rsplit(' ', 1)
            if len(parts) == 2:
                p_type_cpp = parts[0]
                p_name = parts[1]
                params.append({'name': p_name, 'type': cpp_type_to_lua(p_type_cpp)})
            else:
                # Just type? or Just name? Assume just type?
                # The generator produces "Type Name", but if name missing "Type arg"
                pass 
                
    return {'ret': ret_lua, 'params': params}

def parse_file(file_path):
    if not os.path.exists(file_path):
        print(f"Warning: File {file_path} not found.")
        return {}, {}

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # We do NOT strip comments here because we rely on them for types
    
    classes = {}
    enums = {}

    # Regex patterns
    # Matches: .Begin("ClassName")
    begin_pattern = re.compile(r'\.Begin\s*\(\s*"(\w+)"')
    
    # Matches: .BindMemFn("Name", &Class::Fn) // Ret(Args...)
    # We capture: Name, Comment
    bind_mem_fn = re.compile(r'\.BindMemFn\s*\(\s*"(\w+)"[^)]+\)(.*)')
    bind_static_fn = re.compile(r'\.BindStaticFn\s*\(\s*"(\w+)"[^)]+\)(.*)')
    
    # BindFn usually doesn't have generated comments easily unless we manually added them 
    # for lambdas in manual bindings. For now, capture if present.
    bind_fn = re.compile(r'\.BindFn\s*\(\s*"(\w+)"[^)]+\)(.*)')
    
    # Properties
    bind_prop = re.compile(r'\.BindProperty\s*\(\s*"(\w+)"[^)]+\)(.*)')
    
    end_pattern = re.compile(r'\.End\s*\(\s*\)')

    # Enum Pattern
    # // Bind Enum EnumName
    # ...
    # lua_setfield(L, -2, "EnumName");
    enum_start_pattern = re.compile(r'// Bind Enum (\w+)')
    enum_field_pattern = re.compile(r'lua_setfield\(L, -2, "(\w+)"\);')

    # Find all starts
    starts = [(m.group(1), m.end()) for m in begin_pattern.finditer(content)]
    
    for i, (class_name, start_idx) in enumerate(starts):
        search_area = content[start_idx:]
        end_match = end_pattern.search(search_area)
        
        if not end_match:
            continue
            
        block_content = search_area[:end_match.start()]
        
        methods = []
        properties = []
        
        # Parse Member Functions
        for m in bind_mem_fn.finditer(block_content):
            sig = parse_signature(m.group(2))
            methods.append({'name': m.group(1), 'type': 'instance', 'sig': sig})
            
        # Parse Static Functions
        for m in bind_static_fn.finditer(block_content):
            sig = parse_signature(m.group(2))
            methods.append({'name': m.group(1), 'type': 'static', 'sig': sig})
            
        # Parse Generic Functions (BindFn)
        for m in bind_fn.finditer(block_content):
            fn_name = m.group(1)
            fn_type = 'static' if fn_name == 'New' else 'instance'
            # BindFn comments might be missing or manual.
            sig = parse_signature(m.group(2))
            methods.append({'name': fn_name, 'type': fn_type, 'sig': sig})
            
        # Parse Properties
        for m in bind_prop.finditer(block_content):
            sig = parse_signature(m.group(2)) # Returns {'type': ...}
            prop_type = sig.get('type', 'any')
            properties.append({'name': m.group(1), 'type': prop_type})
            
        classes[class_name] = {
            'methods': methods,
            'properties': properties
        }

    # Parse Enums
    # We scan for start markers, then look ahead until we find the closing setfield
    enum_starts = [(m.group(1), m.end()) for m in enum_start_pattern.finditer(content)]
    for enum_name, start_idx in enum_starts:
        # We need to find where this block ends.
        # It ends when we see `lua_setfield(L, -2, "{enum_name}");`
        # We can scan line by line or search
        
        # Simple search for the closing tag
        closing_tag = f'lua_setfield(L, -2, "{enum_name}");'
        end_idx = content.find(closing_tag, start_idx)
        
        if end_idx == -1:
            continue
            
        block_content = content[start_idx:end_idx]
        
        fields = []
        for m in enum_field_pattern.finditer(block_content):
            field_name = m.group(1)
            # Avoid duplicate if any, or self-reference (shouldn't be in block content anyway)
            if field_name != enum_name:
                fields.append(field_name)
        
        enums[enum_name] = fields
        
    return classes, enums

def generate_lua(classes, enums):
    lines = []
    lines.append("---@meta")
    lines.append("-- GENERATED FILE - DO NOT EDIT")
    lines.append("-- This file provides LuaLS annotations for WeilanEngine C++ bindings.")
    lines.append("")
    lines.append("---@class wl")
    lines.append("wl = {}")
    lines.append("")
    
    # Generate Enums
    for enum_name, fields in enums.items():
        lines.append(f"---@class wl.{enum_name}")
        for f in fields:
            lines.append(f"---@field {f} number")
        lines.append(f"wl.{enum_name} = {{}}")
        lines.append("")

    for class_name, data in classes.items():
        # Define the class type
        lines.append(f"---@class wl.{class_name}")
        for prop in data['properties']:
            # prop is dict {'name':..., 'type':...}
            lines.append(f"---@field {prop['name']} {prop['type']}")
        lines.append(f"wl.{class_name} = {{}}")
        lines.append("")
        
        full_class_name = f"wl.{class_name}"
        for method in data['methods']:
            m_name = method['name']
            m_type = method['type']
            m_sig = method.get('sig', {})
            
            # Param annotations
            params_str = "..."
            if m_sig and m_sig.get('params'):
                p_list = []
                for p in m_sig['params']:
                    lines.append(f"---@param {p['name']} {p['type']}")
                    p_list.append(p['name'])
                params_str = ", ".join(p_list)
            
            # Return annotation
            if m_sig and m_sig.get('ret'):
                lines.append(f"---@return {m_sig['ret']}")
            
            if m_type == 'static':
                lines.append(f"function {full_class_name}.{m_name}({params_str}) end")
            else:
                lines.append(f"function {full_class_name}:{m_name}({params_str}) end")
        lines.append("")
        
    return "\n".join(lines)

def main():
    print("Generating Lua Annotations...")
    all_classes = {}
    all_enums = {}
    for f in INPUT_FILES:
        print(f"Parsing {f}...")
        cls, enums = parse_file(f)
        all_classes.update(cls)
        all_enums.update(enums)
    
    lua_code = generate_lua(all_classes, all_enums)
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    
    with open(OUTPUT_FILE, 'w') as f:
        f.write(lua_code)
    print(f"Successfully generated {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
