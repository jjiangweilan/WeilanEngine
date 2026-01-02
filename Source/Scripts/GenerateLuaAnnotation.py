import re
import os

# Configuration
OUTPUT_FILE = os.path.join("Source", "Lua", "weilan_api.lua")
INPUT_FILES = [
    os.path.join("Source", "Engine", "CodeGen", "LuaBindings_Generated.cpp"),
    os.path.join("Source", "Engine", "Runtime", "System", "ScriptingBackend", "LuaBindings.cpp")
]

def strip_comments(content):
    # Remove single line comments
    content = re.sub(r'//.*', '', content)
    # Remove multi-line comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    return content

def parse_file(file_path):
    if not os.path.exists(file_path):
        print(f"Warning: File {file_path} not found.")
        return {}

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    content = strip_comments(content)
    classes = {}

    # Find all .Begin("ClassName") blocks
    # We iterate through matches and scan forward until .End()
    
    # Regex to find the start of a binding block
    # Matches: .Begin("ClassName"
    begin_pattern = re.compile(r'\.Begin\s*\(\s*"(\w+)"')
    
    # Regex for bindings inside the block
    bind_mem_fn = re.compile(r'\.BindMemFn\s*\(\s*"(\w+)"')
    bind_static_fn = re.compile(r'\.BindStaticFn\s*\(\s*"(\w+)"')
    bind_fn = re.compile(r'\.BindFn\s*\(\s*"(\w+)"')
    bind_prop = re.compile(r'\.BindProperty\s*\(\s*"(\w+)"')
    
    end_pattern = re.compile(r'\.End\s*\(\s*\)')

    # Find all starts
    starts = [(m.group(1), m.end()) for m in begin_pattern.finditer(content)]
    
    for i, (class_name, start_idx) in enumerate(starts):
        # Determine the search area for this block.
        # It goes until the next .Begin or the end of file, 
        # but logically it ends at the first .End() found.
        
        # We search for the first .End() after start_idx
        search_area = content[start_idx:]
        end_match = end_pattern.search(search_area)
        
        if not end_match:
            print(f"Warning: Could not find .End() for class {class_name}")
            continue
            
        block_content = search_area[:end_match.start()]
        
        methods = []
        properties = []
        
        # Parse Member Functions
        for m in bind_mem_fn.finditer(block_content):
            methods.append({'name': m.group(1), 'type': 'instance'})
            
        # Parse Static Functions
        for m in bind_static_fn.finditer(block_content):
            methods.append({'name': m.group(1), 'type': 'static'})
            
        # Parse Generic Functions (BindFn)
        for m in bind_fn.finditer(block_content):
            fn_name = m.group(1)
            # Heuristic: 'New' is static, others are instance
            fn_type = 'static' if fn_name == 'New' else 'instance'
            methods.append({'name': fn_name, 'type': fn_type})
            
        # Parse Properties
        for m in bind_prop.finditer(block_content):
            properties.append(m.group(1))
            
        classes[class_name] = {
            'methods': methods,
            'properties': properties
        }
        
    return classes

def generate_lua(classes):
    lines = []
    lines.append("---@meta")
    lines.append("-- GENERATED FILE - DO NOT EDIT")
    lines.append("-- This file provides LuaLS annotations for WeilanEngine C++ bindings.")
    lines.append("")
    lines.append("---@class wl")
    lines.append("wl = {}")
    lines.append("")
    
    for class_name, data in classes.items():
        # Define the class type
        lines.append(f"---@class wl.{class_name}")
        for prop in data['properties']:
            lines.append(f"---@field {prop} any")
        lines.append(f"wl.{class_name} = {{}}")
        lines.append("")
        
        full_class_name = f"wl.{class_name}"
        for method in data['methods']:
            m_name = method['name']
            m_type = method['type']
            
            # Simple heuristic for return types could be added if we had more info.
            # For now, we use (...) and assume any return.
            
            if m_type == 'static':
                lines.append(f"function {full_class_name}.{m_name}(...) end")
            else:
                lines.append(f"function {full_class_name}:{m_name}(...) end")
        lines.append("")
        
    return "\n".join(lines)

def main():
    print("Generating Lua Annotations...")
    all_classes = {}
    for f in INPUT_FILES:
        print(f"Parsing {f}...")
        cls = parse_file(f)
        all_classes.update(cls)
    
    lua_code = generate_lua(all_classes)
    
    # Ensure output directory exists
    os.makedirs(os.path.dirname(OUTPUT_FILE), exist_ok=True)
    
    with open(OUTPUT_FILE, 'w') as f:
        f.write(lua_code)
    print(f"Successfully generated {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
