# Weilan Engine

## Tool Requirements
1. cmake
2. make
3. VulkanSDK
4. Git
5. Xcode (on MacOS)

## Build and Compile Tools
- [x] Windows: MSVC
- [x] Macos: Clang 15+
- [x] iOS: Clang 15+

## How to Build
The project uses a super-build system for it's dependencies. The only dependency you need to install manually is VulkanSDK. Others are downloaded and installed automatically by CMake into the local project folder.<br>

note: after the first successful build, you can set `INSTALL_DEPENDENCY` to `OFF`. I recommand set set build type to RelWithDebugInfo

### Windows/MacOS
1. `mkdir build`
2. `cd build`
3. `cmake -DINSTALL_DEPENDENCY=ON ..`
4. `cmake --build . --target WeilanEngineEditor -j12`
### iOS
1. `mkdir build`
2. `cd build`
3. `cmake -DINSTALL_DEPENDENCY=ON -G"Xcode" -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="<DEVELOPMENT TEAM ID>" -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="Apple Development" -DCMAKE_OSX_SYSROOT=iphoneos ..`
4. Open generated Xcode project and run WeilanEngineEditor

once the dependencies are build you may need to manually codesign libktx.4.dylib and libshaderc_shared.1.dylib under build/lib

e.g: codesign --force --sign <SIGN ID> libshaderc_shared.1.dylib
