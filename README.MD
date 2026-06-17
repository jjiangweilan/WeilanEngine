# Weilan Engine 

## Tool Requirements
1. CMake 3.18+
2. Git with network access for recursive FetchContent/submodule checkout
3. Python 3 with `venv` support
4. Vulkan SDK
5. LuaRocks
6. A C++20 compiler

### macOS Environment
- Install the Vulkan SDK and make sure `VULKAN_SDK` points to the valid SDK `macOS` directory.
- Install LuaRocks and make sure `luarocks` is available on `PATH`.
- CMake creates a build-local Python virtual environment at `build/python-venv` and installs `tree-sitter`, `tree-sitter-cpp`, and `PyYAML` into it.
- Boost is fetched from the Boost Git superproject with recursive submodules enabled. Make sure Git can fetch submodules from GitHub during CMake configure.
- LuaJIT is built with `make`. If `CMAKE_OSX_DEPLOYMENT_TARGET` is not set, the build defaults `MACOSX_DEPLOYMENT_TARGET` to `11.0`.

## Build and Compile Tools
- [x] Windows: MSVC
- [x] Macos: Clang 12+

### Windows
1. `mkdir build`
2. `cd build`
3. `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..`
4. `cmake --build . --target WeilanEngineEditor -j12`<br>

### MacOS
If you are on Apple Silicon you need to disable KTX Software's SSE by setting `-DBASISU_SUPPORT_SSE=OFF`.

Example:
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBASISU_SUPPORT_SSE=OFF
cmake --build build --config RelWithDebInfo --target WeilanEngineEditor -j12
```

## Run the engine
Once you build the engine, execute WeilanEngineEditor.exe with --project argument. Make sure the root path is the source path
   ```
   cd .\WeilanEngine
   .\build\RelWithDebInfo\WeilanEngineEditor --project "A/Existing/Path/To/An/Empty/Folder"
   ```
