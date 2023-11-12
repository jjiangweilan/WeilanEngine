# Weilan Engine

## Tool Requirements
1. cmake
2. make
3. VulkanSDK
4. Git

## Build and Compile Tools
- [x] Windows: MSVC
- [ ] Macos: Clang 12+

## How to Build
The project uses a super-build system for it's dependencies. The only dependency you need to install manually is VulkanSDK. Others are downloaded and installed automatically by CMake into the local project folder.<br>
The build has some side-effect of installing resources into your system's data folder (~/.config/WeilaEngine on MacOS and $LOCALAPPDATA$/WeilanEngine on Windows)

note: after the first successful build, you can set `INSTALL_DEPENDENCE` to `OFF`. I recommand set shaderc's build type to RelWithDebugInfo, it's too slow in debug mode.

### Windows
1. `mkdir build`
2. `cd build`
3. `cmake -DINSTALL_DEPENDENCY=ON ..`
4. `cmake --build . --target WeilanEngineEditor -j12`<br>

### MacOS
if you are on apple silicon you need to disable ktx software's SSE by setting `-DBASISU_SUPPORT_SSE=OFF`
