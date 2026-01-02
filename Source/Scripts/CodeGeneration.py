import os
import sys
import filecmp
import shutil

# Ensure we can import the generator scripts located in the same directory
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.append(current_dir)

import GenerateLuaBindings
import GenerateSerialization
import GenerateTypeReflection

def compare_and_replace(temp_path, target_path):
    """
    Compares temp_path and target_path.
    If target_path doesn't exist or content differs, replaces target_path with temp_path.
    Otherwise, removes temp_path.
    """
    if not os.path.exists(temp_path):
        print(f"Error: Temporary file {temp_path} was not generated.")
        return

    if not os.path.exists(target_path):
        print(f"Creating {target_path}...")
        os.makedirs(os.path.dirname(target_path), exist_ok=True)
        shutil.move(temp_path, target_path)
    else:
        # Compare files
        if filecmp.cmp(temp_path, target_path, shallow=False):
            # Files are identical
            print(f"Up-to-date: {target_path}")
            os.remove(temp_path)
        else:
            # Files differ
            print(f"Updating {target_path}...")
            # Remove target first to avoid permission issues
            if os.path.exists(target_path):
                os.remove(target_path)
            shutil.move(temp_path, target_path)

def fix_serialization_includes(temp_cpp_path, target_hpp_name):
    """
    GenerateSerialization.py generates an include for the header based on the output filename.
    If we output to .cpp.tmp, it includes .hpp.tmp. We need to fix this to point to the real .hpp.
    """
    if not os.path.exists(temp_cpp_path):
        return

    # The generator produces #include "Target.hpp.tmp"
    # We want #include "Target.hpp"
    
    wrong_include = f'#include "{target_hpp_name}.tmp"'
    correct_include = f'#include "{target_hpp_name}"'
    
    with open(temp_cpp_path, 'r') as f:
        content = f.read()
    
    if wrong_include in content:
        content = content.replace(wrong_include, correct_include)
        with open(temp_cpp_path, 'w') as f:
            f.write(content)

def main():
    source_root = "Source"
    
    # Task list: (Module, Function, OutputPath, Options)
    # Note: Paths are relative to project root
    tasks = [
        {
            "module": GenerateLuaBindings,
            "func": "generate_bindings",
            "output": "Source/Engine/CodeGen/LuaBindings_Generated.cpp"
        },
        {
            "module": GenerateTypeReflection,
            "func": "generate_reflection_code",
            "output": "Source/Engine/CodeGen/TypeReflection_Generated.cpp"
        },
        {
            "module": GenerateSerialization,
            "func": "generate_serialization_code",
            "output": "Source/Engine/CodeGen/Serialization_Generated.cpp",
            "is_serialization": True
        }
    ]

    for task in tasks:
        module = task["module"]
        func_name = task["func"]
        target_output = task["output"]
        temp_output = target_output + ".tmp"
        
        print(f"Running {module.__name__}...")
        
        # Get the function
        func = getattr(module, func_name)
        
        # Run generation
        try:
            func(source_root, temp_output)
        except Exception as e:
            print(f"Error running {func_name}: {e}")
            if os.path.exists(temp_output):
                os.remove(temp_output)
            continue
            
        # Special handling for Serialization
        if task.get("is_serialization"):
            # It generates .cpp.tmp and .hpp.tmp
            target_hpp = target_output.replace(".cpp", ".hpp")
            # GenerateSerialization replaces .cpp with .hpp in the string passed to it.
            # So ...Generated.cpp.tmp -> ...Generated.hpp.tmp
            temp_hpp = temp_output.replace(".cpp", ".hpp") 
            
            # Fix include in cpp
            target_hpp_name = os.path.basename(target_hpp)
            fix_serialization_includes(temp_output, target_hpp_name)
            
            # Compare and replace CPP
            compare_and_replace(temp_output, target_output)
            
            # Compare and replace HPP
            compare_and_replace(temp_hpp, target_hpp)
        else:
            # Standard single file comparison
            compare_and_replace(temp_output, target_output)

if __name__ == "__main__":
    main()
