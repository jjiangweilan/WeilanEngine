# Weilan Engine

## Tool Requirements
1. cmake
2. make
3. VulkanSDK
4. Git

## Build and Compile Tools
- [x] Windows: MSVC
- [x] Macos: Clang 12+

## How to Build
The project uses a super-build system for it's dependencies. The only dependency you need to install manually is VulkanSDK. Others are downloaded and installed automatically by CMake into the local project folder.<br>
The build has some side-effect of installing resources into your system's data folder (~/.config/WeilaEngine on MacOS and $LOCALAPPDATA$/WeilanEngine on Windows)

1. `mkdir build`
2. `cd build`
3. `cmake -DINSTALL_DEPENDENCE=ON -G"Unix Makefiles" .. `
4. `cmake --build . --target WeilanGame`<br>
(note: after the first successful build, you can set `INSTALL_DEPENDENCE` to `OFF`)
