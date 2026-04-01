#!/usr/bin/env python3
"""
Shader Compilation Script for WeilanEngine

This script compiles all .slang shaders that contain entry points,
caches the results (SPV binaries, pipeline info, config), and generates
a manifest for the engine to load at runtime.
"""

import os
import sys
import json
import hashlib
import re
import subprocess
import shutil
import itertools
import concurrent.futures
from pathlib import Path
from typing import Optional

import yaml

# Paths
SCRIPT_DIR = Path(__file__).parent.resolve()
ENGINE_ROOT = SCRIPT_DIR.parent.parent
SHADER_ROOT = ENGINE_ROOT / "Source" / "Engine" / "Shaders"
OUTPUT_ROOT = ENGINE_ROOT / "Source" / "CompiledShader"
BUILD_DIR = ENGINE_ROOT / "build"

# Find ShaderCompilerTool executable
def find_shader_compiler_tool() -> Path:
    """Find ShaderCompilerTool executable in build directory."""
    # Check common build output locations
    possible_paths = [
        BUILD_DIR / "RelWithDebInfo" / "ShaderCompilerTool.exe",
        BUILD_DIR / "Release" / "ShaderCompilerTool.exe",
        BUILD_DIR / "Debug" / "ShaderCompilerTool.exe",
        BUILD_DIR / "ShaderCompilerTool.exe",
        BUILD_DIR / "ShaderCompilerTool",
        BUILD_DIR / "Release" / "ShaderCompilerTool",
        BUILD_DIR / "Debug" / "ShaderCompilerTool",
    ]
    
    for path in possible_paths:
        if path.exists():
            return path
    
    # Fall back to searching
    for root, dirs, files in os.walk(BUILD_DIR):
        for f in files:
            if f == "ShaderCompilerTool.exe" or f == "ShaderCompilerTool":
                return Path(root) / f
    
    raise FileNotFoundError("ShaderCompilerTool not found in build directory. Please build the project first.")

SHADER_COMPILER_TOOL = None  # Will be set in main()
SLANGC = None  # For preprocessing, will be set in main()

# Find slangc executable (still needed for preprocessing)
def find_slangc() -> Optional[Path]:
    """Find slangc executable in build directory."""
    for root, dirs, files in os.walk(BUILD_DIR / "_deps"):
        for f in files:
            if f == "slangc.exe" or f == "slangc":
                return Path(root) / f
    return None

# Regex patterns
# Note: Preprocessed output may have spaces around tokens, so patterns need to be flexible
ENTRY_POINT_PATTERN = re.compile(r'\[\s*shader\s*\(\s*["\'](\w+)["\']\s*\)\s*\]')
FEATURE_PATTERN = re.compile(r'static\s+extern\s+const\s+bool\s+(\w+)\s*=\s*(true|false)\s*;')
IMPORT_PATTERN = re.compile(r'import\s+([\w.]+)\s*;')
INCLUDE_PATTERN = re.compile(r'#include\s+[<"]([^>"]+)[>"]')
CONFIG_BLOCK_PATTERN = re.compile(r'#(?:if|ifdef)\s+CONFIG\s*(.*?)#endif', re.DOTALL)


def preprocess_shader(filepath: Path) -> Optional[str]:
    """Preprocess shader using slangc -E to resolve imports/includes."""
    if SLANGC is None:
        return None
    
    cmd = [
        str(SLANGC),
        "-I", str(SHADER_ROOT),
        "-E",  # Preprocess only
        "-D", "CONFIG=0",
        "-D", "GPU_RESOURCE=1",
        str(filepath)
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if result.returncode == 0:
            return result.stdout
        else:
            # Fall back to reading raw file if preprocessing fails
            return None
    except Exception:
        return None


def extract_entry_points_from_content(content: str) -> dict[str, str]:
    """Extract entry point names and their shader stages from content string."""
    entry_points = {}
    
    # For preprocessed content (single line), find all [shader("stage")] patterns
    # and the function name that follows (may have other attributes like [numthreads] in between)
    # Handles both single [shader(...)] and double [ [ shader(...) ] ] bracket notation (with spaces)
    # Pattern: [shader("stage")] ... FunctionName( or [ [ shader("stage") ] ] ... FunctionName(
    pattern = re.compile(r'\[\s*\[?\s*shader\s*\(\s*["\'](\w+)["\']\s*\)\s*\]?\s*\](?:\s*\[[^\]]*\]\s*)*\s*\w+\s+(\w+)\s*\(')
    for match in pattern.finditer(content):
        stage = match.group(1).lower()
        func_name = match.group(2)
        entry_points[stage] = func_name
    
    return entry_points


def compute_file_hash(filepath: Path) -> str:
    """Compute SHA256 hash of file content."""
    sha256 = hashlib.sha256()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), b''):
            sha256.update(chunk)
    return sha256.hexdigest()


def compute_content_hash(filepath: Path, dependencies: list[str]) -> str:
    """Compute hash of preprocessed shader content (includes all dependencies)."""
    sha256 = hashlib.sha256()
    
    # we can't rely on preprocessed content here, because preprocessed content doesn't handle import modules, so hash raw file and all dependencies
    with open(filepath, 'rb') as f:
        sha256.update(f.read())
    
    for dep in sorted(dependencies):
        dep_path = SHADER_ROOT / dep
        if dep_path.exists():
            with open(dep_path, 'rb') as f:
                sha256.update(f.read())
    
    return sha256.hexdigest()


def resolve_import_path(import_name: str) -> Optional[Path]:
    """Resolve an import name to a file path."""
    # Convert dot notation to path
    rel_path = import_name.replace('.', '/')
    
    # Try .slang extension
    slang_path = SHADER_ROOT / f"{rel_path}.slang"
    if slang_path.exists():
        return slang_path
    
    # Try .hlsl extension
    hlsl_path = SHADER_ROOT / f"{rel_path}.hlsl"
    if hlsl_path.exists():
        return hlsl_path
    
    return None


def collect_dependencies(filepath: Path, visited: Optional[set] = None) -> list[str]:
    """Recursively collect all import/include dependencies."""
    if visited is None:
        visited = set()
    
    if str(filepath) in visited:
        return []
    visited.add(str(filepath))
    
    dependencies = []
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception:
        return []
    
    # Find imports
    for match in IMPORT_PATTERN.finditer(content):
        import_name = match.group(1)
        
        dep_path = resolve_import_path(import_name)
        if dep_path:
            try:
                rel_str = dep_path.resolve().relative_to(SHADER_ROOT.resolve()).as_posix()
                dependencies.append(rel_str)
            except ValueError:
                pass
            dependencies.extend(collect_dependencies(dep_path, visited))
    
    # Find includes
    for match in INCLUDE_PATTERN.finditer(content):
        include_path = match.group(1)
        
        full_path = filepath.parent / include_path
        if not full_path.exists():
            full_path = SHADER_ROOT / include_path
        
        if full_path.exists():
            try:
                rel_str = full_path.resolve().relative_to(SHADER_ROOT.resolve()).as_posix()
                dependencies.append(rel_str)
            except ValueError:
                pass
            dependencies.extend(collect_dependencies(full_path, visited))
    
    return list(set(dependencies))


def extract_entry_points(filepath: Path) -> dict[str, str]:
    """Extract entry point names and their shader stages from preprocessed shader."""
    # Try preprocessed content first
    content = preprocess_shader(filepath)
    if content:
        return extract_entry_points_from_content(content)
    
    # Fall back to raw file parsing
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    entry_points = {}
    lines = content.split('\n')
    
    for i, line in enumerate(lines):
        match = ENTRY_POINT_PATTERN.search(line)
        if match:
            stage = match.group(1).lower()
            # Find the function name on the next line(s)
            for j in range(i + 1, min(i + 5, len(lines))):
                func_match = re.search(r'^\s*\w+\s+(\w+)\s*\(', lines[j])
                if func_match:
                    func_name = func_match.group(1)
                    entry_points[stage] = func_name
                    break
    
    return entry_points


def extract_features(filepath: Path) -> list[dict]:
    """Extract static extern const bool features from preprocessed shader."""
    # Try preprocessed content first
    content = preprocess_shader(filepath)
    if content is None:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    
    features = []
    # Pattern handles both normal and preprocessed (spaced) content
    pattern = re.compile(r'static\s+extern\s+const\s+bool\s+(\w+)\s*=\s*(true|false)\s*;')
    for match in pattern.finditer(content):
        name = match.group(1)
        default_value = match.group(2) == 'true'
        features.append({'name': name, 'defaultValue': default_value})
    
    return features


def extract_pipeline_config(filepath: Path) -> dict:
    """Extract pipeline config from #ifdef CONFIG block using PyYAML."""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    match = CONFIG_BLOCK_PATTERN.search(content)
    if not match:
        return {}
    
    config_content = match.group(1).strip()
    if not config_content:
        return {}
    
    try:
        config = yaml.safe_load(config_content)
        return convert_config_to_engine_format(config) if config else {}
    except yaml.YAMLError as e:
        print(f"Warning: Failed to parse CONFIG block in {filepath}: {e}")
        return {}


def convert_config_to_engine_format(config: dict) -> dict:
    """Convert YAML config to engine's PipelineConfig JSON format."""
    result = {
        "cullMode": 2,  # Back
        "topology": 0,  # TriangleList
        "polygonMode": 0,  # Fill
        "depth": {
            "writeEnable": True,
            "testEnable": True,
            "compOp": 6,  # Greater_Or_Equal
            "boundTestEnable": False,
            "minBounds": 0.0,
            "maxBounds": 1.0
        },
        "stencil": {
            "testEnable": False,
            "front": {"failOp": 0, "passOp": 0, "depthFailOp": 0, "compareOp": 7, "compareMask": 0, "writeMask": 0, "reference": 0},
            "back": {"failOp": 0, "passOp": 0, "depthFailOp": 0, "compareOp": 7, "compareMask": 0, "writeMask": 0, "reference": 0}
        },
        "color": {
            "blends": [],
            "blendConstants": [1.0, 1.0, 1.0, 1.0]
        }
    }
    
    # Map cull mode - handle both string and boolean (YAML parses 'off' as False)
    cull_map = {"none": 0, "off": 0, "front": 1, "back": 2, "both": 3}
    if "cull" in config:
        cull_val = config["cull"]
        if isinstance(cull_val, bool):
            result["cullMode"] = 0 if not cull_val else 2  # False/off -> none, True -> back
        elif isinstance(cull_val, str):
            result["cullMode"] = cull_map.get(cull_val.lower(), 2)
    
    # Map topology
    topology_map = {"trianglelist": 0, "trianglestrip": 1, "linestrip": 2, "linelist": 3}
    if "topology" in config:
        topo_val = config["topology"]
        if isinstance(topo_val, str):
            result["topology"] = topology_map.get(topo_val.lower(), 0)
    
    # Map polygon mode
    polygon_map = {"fill": 0, "line": 1, "point": 2}
    if "polygonMode" in config:
        poly_val = config["polygonMode"]
        if isinstance(poly_val, str):
            result["polygonMode"] = polygon_map.get(poly_val.lower(), 0)
    
    # Depth settings
    if "depth" in config:
        depth = config["depth"]
        if isinstance(depth, dict):
            result["depth"]["testEnable"] = depth.get("testEnable", True)
            result["depth"]["writeEnable"] = depth.get("writeEnable", True)
            
            comp_map = {"never": 0, "less": 1, "equal": 2, "lessorequal": 3, "greater": 4, "notequal": 5, "greaterorequal": 6, "always": 7}
            if "compOp" in depth:
                comp_val = depth["compOp"]
                if isinstance(comp_val, str):
                    result["depth"]["compOp"] = comp_map.get(comp_val.lower().replace("_", ""), 6)
    
    # Blend settings
    if "blend" in config:
        blend_list = config["blend"] if isinstance(config["blend"], list) else [config["blend"]]
        for blend_str in blend_list:
            if blend_str and isinstance(blend_str, str):
                blend_state = parse_blend_string(blend_str)
                result["color"]["blends"].append(blend_state)
    
    # Color mask
    if "mask" in config:
        mask_str = config["mask"]
        if isinstance(mask_str, str):
            mask_value = 0
            if 'r' in mask_str.lower(): mask_value |= 1
            if 'g' in mask_str.lower(): mask_value |= 2
            if 'b' in mask_str.lower(): mask_value |= 4
            if 'a' in mask_str.lower(): mask_value |= 8
            
            if result["color"]["blends"]:
                result["color"]["blends"][0]["colorWriteMask"] = mask_value
            else:
                result["color"]["blends"].append({
                    "blendEnable": False,
                    "srcColorBlendFactor": 1,
                    "dstColorBlendFactor": 0,
                    "colorBlendOp": 0,
                    "srcAlphaBlendFactor": 1,
                    "dstAlphaBlendFactor": 0,
                    "alphaBlendOp": 0,
                    "colorWriteMask": mask_value
                })
    
    return result


def parse_blend_string(blend_str: str) -> dict:
    """Parse blend string like 'SrcAlpha OneMinusSrcAlpha'."""
    blend_factor_map = {
        "zero": 0, "one": 1, "srccolor": 2, "oneminussrccolor": 3,
        "dstcolor": 4, "oneminusdstcolor": 5, "srcalpha": 6, "oneminussrcalpha": 7,
        "dstalpha": 8, "oneminusdstalpha": 9
    }
    
    parts = blend_str.lower().split()
    
    result = {
        "blendEnable": True,
        "srcColorBlendFactor": 6,  # SrcAlpha
        "dstColorBlendFactor": 7,  # OneMinusSrcAlpha
        "colorBlendOp": 0,
        "srcAlphaBlendFactor": 6,
        "dstAlphaBlendFactor": 7,
        "alphaBlendOp": 0,
        "colorWriteMask": 15
    }
    
    if len(parts) >= 2:
        result["srcColorBlendFactor"] = blend_factor_map.get(parts[0], 6)
        result["dstColorBlendFactor"] = blend_factor_map.get(parts[1], 7)
        result["srcAlphaBlendFactor"] = result["srcColorBlendFactor"]
        result["dstAlphaBlendFactor"] = result["dstColorBlendFactor"]
    
    if len(parts) >= 4:
        result["srcAlphaBlendFactor"] = blend_factor_map.get(parts[2], 6)
        result["dstAlphaBlendFactor"] = blend_factor_map.get(parts[3], 7)
    
    return result


def has_entry_points(filepath: Path) -> bool:
    """Check if shader file contains entry point definitions."""
    # First check raw file (faster, and slangc may not be available yet)
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    return bool(ENTRY_POINT_PATTERN.search(content))


def discover_shaders() -> list[Path]:
    """Discover all .slang files with entry points."""
    shaders = []
    for slang_file in SHADER_ROOT.rglob("*.slang"):
        if has_entry_points(slang_file):
            shaders.append(slang_file)
    return shaders


def get_shader_name(filepath: Path) -> str:
    """Get shader name relative to shader root without extension."""
    return filepath.relative_to(SHADER_ROOT).with_suffix('').as_posix()


def compile_shader(shader_path: Path) -> bool:
    """Compile a shader with all its permutations."""
    shader_name = get_shader_name(shader_path)
    message = f"Compiling: {shader_name}\n"
    
    output_dir = OUTPUT_ROOT / shader_name.replace('/', os.sep)
    
    # Extract shader information
    entry_points = extract_entry_points(shader_path)
    if not entry_points:
        message += f"  No entry points found, skipping"
        print(message)
        return False
    
    dependencies = collect_dependencies(shader_path)
    source_hash = compute_content_hash(shader_path, dependencies)
    pipeline_config = extract_pipeline_config(shader_path)
    
    # Check if recompilation needed
    shader_meta_path = output_dir / "shader_meta.json"
    if shader_meta_path.exists():
        try:
            with open(shader_meta_path, 'r') as f:
                existing_meta = json.load(f)
            if existing_meta.get("sourceHash") == source_hash:
                message += f"  Up to date (hash match)"
                print(message)
                return True
        except Exception:
            pass
   
    # Clean output directory
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Run ShaderCompilerTool
    cmd = [
        str(SHADER_COMPILER_TOOL),
        "--shader", shader_name,
        "--output", str(output_dir),
        "--shader-root", str(SHADER_ROOT),
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        if result.returncode != 0:
            message += f"  Error compiling {shader_name}:\n"
            message += f"    {result.stderr.strip()}"
            print(message)
            return False
    except subprocess.TimeoutExpired:
        message += f"  Timeout compiling {shader_name}"
        print(message)
        return False
    except Exception as e:
        message += f"  Exception compiling {shader_name}: {e}"
        print(message)
        return False
    
    # Update shader metadata with Python-extracted info
    if not shader_meta_path.exists():
        message += f"  Error: shader_meta.json not generated for {shader_name}"
        print(message)
        return False
        
    try:
        with open(shader_meta_path, 'r') as f:
            shader_meta = json.load(f)
        
        shader_meta["sourceHash"] = source_hash
        shader_meta["dependencies"] = dependencies
        shader_meta["entryPoints"] = entry_points
        shader_meta["pipelineConfig"] = pipeline_config
        
        with open(shader_meta_path, 'w') as f:
            json.dump(shader_meta, f, indent=2)
            
    except Exception as e:
        message += f"  Error updating metadata for {shader_name}: {e}"
        print(message)
        return False
    
    message += f"  Compiled successfully"
    print(message)
    return True


def generate_manifest():
    """Generate the master shader manifest."""
    manifest = {
        "version": 1,
        "shaders": []
    }
    
    # Recursively find all shader_meta.json files
    for meta_path in OUTPUT_ROOT.rglob("shader_meta.json"):
        try:
            with open(meta_path, 'r') as f:
                meta = json.load(f)
            manifest["shaders"].append({
                "name": meta["shaderName"],
                "hash": meta["sourceHash"]
            })
        except Exception as e:
            print(f"Warning: Failed to read {meta_path}: {e}")
    
    manifest_path = OUTPUT_ROOT / "shader_manifest.json"
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"Generated manifest with {len(manifest['shaders'])} shaders")


def main():
    global SHADER_COMPILER_TOOL, SLANGC
    
    print("=== WeilanEngine Shader Compiler ===")
    
    # Find ShaderCompilerTool
    try:
        SHADER_COMPILER_TOOL = find_shader_compiler_tool()
        print(f"Using ShaderCompilerTool: {SHADER_COMPILER_TOOL}")
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    
    # Find slangc for preprocessing (optional, will fall back to raw file parsing)
    SLANGC = find_slangc()
    if SLANGC:
        print(f"Using slangc for preprocessing: {SLANGC}")
    else:
        print("slangc not found, will use raw file parsing for preprocessing")
    
    # Create output directory
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    
    # Discover shaders
    shaders = discover_shaders()
    print(f"Found {len(shaders)} shaders with entry points")
    
    # Compile each shader
    success = 0
    with concurrent.futures.ThreadPoolExecutor() as executor:
        results = list(executor.map(compile_shader, shaders))
    success = sum(results) 
    
    # Generate manifest
    generate_manifest()
    
    print(f"\nCompleted: {success}/{len(shaders)} shaders compiled successfully")
    
    return 0 if success == len(shaders) else 1


if __name__ == "__main__":
    sys.exit(main())
