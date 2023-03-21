add_executable(lightmapper
    main.cpp
    ../src/vk_helpers.h
)

set_property(TARGET lightmapper PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

target_include_directories(lightmapper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(lightmapper PRIVATE "${PROJECT_SOURCE_DIR}/external")
target_include_directories(lightmapper PRIVATE ${Vulkan_INCLUDE_DIRS}) 
target_link_libraries(lightmapper vma tinyobjloader glm spirv-reflect xatlas)
target_link_libraries(lightmapper Vulkan::Vulkan sdl2)
add_dependencies(lightmapper Shaders)