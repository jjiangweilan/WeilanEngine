# WeilanEngine
cmake_minimum_required(VERSION 3.18.0)


#add_custom_target(
# spv2glsl ALL
# COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/python/GLSLProcessor.py
# COMMENT "Generating spv from glsl"
#)

#add_custom_target(
#    copyAsset ALL
#        COMMAND ${CMAKE_COMMAND} -E copy_directory
#        ${CMAKE_CURRENT_LIST_DIR}/../Assets/
#        ${CMAKE_BINARY_DIR}/bin/Assets/
#    DEPENDS spv2glsl
#    COMMENT "copying assets"
#    )
#add_dependencies(WeilanEngine copyAsset)

# move builtin assets files
# if (WIN32)
#     set(BuiltInAssetsDir $ENV{LOCALAPPDATA}/WeilanEngine/Assets/)
# elseif (APPLE)
#     set(BuiltInAssetsDir $ENV{HOME}/.config/WeilanEngine/Assets/)
# endif()
# add_custom_target(copyAssets ALL
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_CURRENT_LIST_DIR}/../Assets
#     ${BuiltInAssetsDir}
# )
# add_dependencies(WeilanEngine copyAssets)

# Unit Test
if (UNIT_TEST)
    add_subdirectory(Test/)
endif()
