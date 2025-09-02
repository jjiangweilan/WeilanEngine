# Weilan Engine 

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

## Run the engine
Once you build the engine, execute WeilanEngineEditor.exe with --project argument. Make sure the root path is the source path
   ```
   cd .\WeilanEngine
   .\build\RelWithDebInfo\WeilanEngineEditor --project "A/Existing/Path/To/An/Empty/Folder"
   ```

