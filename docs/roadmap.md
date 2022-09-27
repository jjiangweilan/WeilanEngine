# Roadmap

* Engine
- [ ] Lua Binding
    * AssetDatabase
        * Importers
            - [x] Shader importer
            - [x] Mesh importer
                - [x] glb
        - [x] AssetObject Serialization support
* Editor
    - [x] AssetInspector
    - [ ] SceneTreeWindow
        - [ ] Editing mode, Game mode
    - [x] AssetExplorer
    - [x] individual project space
    - [x] Game Scene Window
        - [ ] In scene editing handles
    - [x] Internal Resources
    - [ ] Save Scene Tree
* Shader
    - [x] Shader config file (default blending mode, vertex interleaving etc...)
* Drivers
    - [x] Vulkan rendering backend
* CMake
    - [ ] support install command
* misc
    - [ ] We have two "Global" objects the one called Globals need to be rafactored

## TODO
* Editor
    1. ~~Put importers to editor code~~ (importers are needed to load data at runtime, we can't strip importers from build)
    2. Some dependencies are only needed in editor, remove them in build
* Shader
    1. Support macro branching in shader (shaderc supports this, we can use it)
    2. Single file shader (shader stages in one file, together with shader configs). Related: Shader importer
    3. Vulkan Pipeline Cache
    4. I think we need to split push constant update and descriptor set binding (void VKCommandBuffer::BindResource(RefPtr<Gfx::ShaderResource> resource_))
* Render Pass and Frame Buffer
    - [ ] we should be able to induce framebuffer's attachment from the renderpass, so we don't need to manually set them. (it's required to be the same by the spec)
* Global

## bugfix
1. ImGui font size is wrong in Apple MacOS (high DPI)
    * https://github.com/ocornut/imgui/issues/3757
2. We need a way to let RefPtr know it's reference(when it's an UniPtr) is already invalid
