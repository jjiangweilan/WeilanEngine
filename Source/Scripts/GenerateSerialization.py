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
    Checks if a node has a specific attribute (e.g., [[SerClass]]).
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
            if find_attribute(node, code_bytes, 'SerClass'):
                class_name_node = node.child_by_field_name('name')
                if not class_name_node:
                    return # Anonymous class? 

                class_name = get_node_text(class_name_node, code_bytes)
                namespaces = get_namespace_list(node, code_bytes)
                
                # Determine if it's a class or struct
                kind = 'struct'
                if node.type == 'class_specifier':
                    kind = 'class'

                class_info = {
                    'name': class_name,
                    'namespaces': namespaces,
                    'kind': kind,
                    'properties': []
                }

                explicit_props = []
                all_props = []

                # Traverse body for members
                body = node.child_by_field_name('body')
                if body:
                    for member in body.children:
                        if member.type == 'field_declaration':
                            # Check for static
                            is_static = False
                            for child in member.children:
                                if child.type == 'storage_class_specifier' and get_node_text(child, code_bytes) == 'static':
                                    is_static = True
                                    break
                            if is_static:
                                continue

                            # Check if the whole declaration has [[SerProp]]
                            decl_has_attr = find_attribute(member, code_bytes, 'SerProp')

                            # Helper to recursively find field identifiers and check for attributes
                            def find_field_identifiers(n, explicit_inherited):
                                identifiers = []
                                current_explicit = explicit_inherited
                                
                                # Check if this node adds 'SerProp' attribute
                                # Note: find_attribute checks children of n for attribute_declaration
                                if n.type == 'attributed_declarator':
                                    if find_attribute(n, code_bytes, 'SerProp'):
                                        current_explicit = True
                                
                                if n.type == 'field_identifier':
                                    return [{'name': get_node_text(n, code_bytes), 'explicit': current_explicit}]
                                
                                # Recurse
                                for child in n.children:
                                    # Don't recurse into initialization values (e.g. int x = y; ignore y)
                                    if n.type == 'init_declarator' and child.field_name == 'value':
                                        continue
                                    
                                    # Don't recurse into types or attributes themselves to find identifiers
                                    if child.type in ['primitive_type', 'type_identifier', 'template_type', 'attribute_declaration']:
                                        continue
                                        
                                    identifiers.extend(find_field_identifiers(child, current_explicit))
                                return identifiers

                            # Start search from field_declaration children
                            for child in member.children:
                                if child.type in [';', 'storage_class_specifier', 'primitive_type', 'type_identifier', 'template_type', 'attribute_declaration']:
                                    continue
                                
                                found = find_field_identifiers(child, decl_has_attr)
                                for item in found:
                                    all_props.append(item['name'])
                                    if item['explicit']:
                                        explicit_props.append(item['name'])

                if explicit_props:
                    class_info['properties'] = explicit_props
                else:
                    class_info['properties'] = all_props

                classes_found.append(class_info)

        for child in node.children:
            traverse(child)

    traverse(tree.root_node)
    return classes_found

def generate_serialization_code(source_dir, output_file):
    all_classes = []
    
    # Scan all .hpp files
    files = glob.glob(os.path.join(source_dir, '**/*.hpp'), recursive=True)
    
    for file_path in files:
        classes = process_file(file_path)
        if classes:
            # Calculate relative include path
            rel_path = os.path.relpath(file_path, os.path.dirname(output_file)).replace('\\', '/')
            all_classes.append({'include': rel_path, 'classes': classes})

    # Generate .cpp
    out_cpp = []
    out_cpp.append('// GENERATED FILE - DO NOT EDIT')
    out_cpp.append('// This file registers serialization for classes marked with [[SerClass]]')
    out_cpp.append('')
    out_cpp.append('#include "Engine/Library/Serialization/Serializer.hpp"')
    
    header_path = output_file.replace('.cpp', '.hpp')
    header_name = os.path.basename(header_path)
    out_cpp.append(f'#include "{header_name}"')
    out_cpp.append('')
    
    for entry in all_classes:
        out_cpp.append(f'#include "{entry["include"]}"')
    
    out_cpp.append('')
    
    # Generate .hpp
    out_hpp = []
    out_hpp.append('// GENERATED FILE - DO NOT EDIT')
    out_hpp.append('#pragma once')
    out_hpp.append('')
    out_hpp.append('class Serializer;')
    out_hpp.append('')

    for entry in all_classes:
        for cls in entry['classes']:
            name = cls['name']
            namespaces = cls['namespaces']
            kind = cls.get('kind', 'struct') # Default to struct if missing
            
            # Open namespaces in CPP
            for ns in namespaces:
                out_cpp.append(f'namespace {ns} {{')
            
            # Serialize CPP
            out_cpp.append(f'void Serialize(Serializer* s, const {name}* val)')
            out_cpp.append('{')
            if cls['properties']:
                for prop in cls['properties']:
                    out_cpp.append(f'    s->Serialize("{prop}", val->{prop});')
            out_cpp.append('}')
            out_cpp.append('')

            # Deserialize CPP
            out_cpp.append(f'void Deserialize(Serializer* s, {name}* val)')
            out_cpp.append('{')
            if cls['properties']:
                for prop in cls['properties']:
                    out_cpp.append(f'    s->Deserialize("{prop}", val->{prop});')
            out_cpp.append('}')
            out_cpp.append('')

            # Close namespaces in CPP
            for ns in reversed(namespaces):
                out_cpp.append(f'}} // namespace {ns}')
            out_cpp.append('')

            # Declarations in HPP
            for ns in namespaces:
                out_hpp.append(f'namespace {ns} {{')
            
            indent = '    ' * len(namespaces)
            out_hpp.append(f'{indent}{kind} {name};')
            out_hpp.append(f'{indent}void Serialize(Serializer* s, const {name}* val);')
            out_hpp.append(f'{indent}void Deserialize(Serializer* s, {name}* val);')
            
            for ns in reversed(namespaces):
                out_hpp.append(f'}} // namespace {ns}')
            out_hpp.append('')

    # Ensure directory exists
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'w') as f:
        f.write('\n'.join(out_cpp))
    
    with open(header_path, 'w') as f:
        f.write('\n'.join(out_hpp))
    
    print(f'Generated serialization code for {len(all_classes)} files to {output_file} and {header_path}')

if __name__ == "__main__":
    source_root = "Source"
    output_path = "Source/Engine/CodeGen/Serialization_Generated.cpp"
    generate_serialization_code(source_root, output_path)
