# Weilan Engine

## Get Started
1. If you enter the engine for the first time use `Create New Project` in side Projects window.
Enter your project path and click `Confirm`
2. Click `Create Scene` on the top left
3. Then you can right click in Scene Tree Window to create an empty object
4. Select your newly created object, right click in Inspector to create a MeshRenderer
5. You can now copy your mesh file into Assets folder and drag drop it into inspector
6. Create a new object and create a camera component to your new object
7. Create a material object and type `simple` in it's Shader property. `simple` is a builtin Shader. Drag and drop it into the MeshRenderer
8. You can now click `Editor Game` to move around the scene

## Tool Requirements
1. cmake
2. make
3. VulkanSDK
4. Git

## Build and Compile Tools
- [x] Windows: MSVC
- [x] Macos: Clang 12+

### Windows
1. `mkdir build`
2. `cd build`
3. `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..`
4. `cmake --build . --target WeilanEngineEditor -j12`<br>

### MacOS
if you are on apple silicon you need to disable ktx software's SSE by setting `-DBASISU_SUPPORT_SSE=OFF`
