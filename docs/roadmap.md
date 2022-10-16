# Roadmap

## TODO
Core
- [ ] Lua Binding
- [ ] Physics (Bullet)

Editor
- [ ] In scene editing handles (Move, Rotate, Scale)
- [ ] Editing mode, Game mode
- [x] Shader importer (custom shader)
- [x] Mesh importer (.glb)
- [x] AssetObject Serialization support
- [x] AssetInspector
- [x] SceneTreeWindow
- [x] AssetExplorer
- [x] individual project space
- [x] Game Scene Window
- [x] Internal Resources
- [x] Save Scene Tree
- [x] Shader config file (default blending mode, vertex interleaving etc...)

Drivers
- [ ] RenderPass should be able to set multiple depth texture
- [x] Set push constant in GfxDriver API. Currently we can only update implicitly through ShaderResource
- [x] Vulkan rendering backend

CMake
- [ ] support install command

misc
- [ ] We have two "Global" objects the one called Globals need to be rafactored
- [ ] ~~Put importers to editor code~~ (importers are needed to load data at runtime, we can't strip importers from build)
- [ ] Some dependencies are only needed in editor, remove them in build

## Bugfix
[x] ImGui font size is wrong in Apple MacOS (high DPI)
    * https://github.com/ocornut/imgui/issues/3757
    2. We need a way to let RefPtr know it's reference(when it's an UniPtr) is already invalid


