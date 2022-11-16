add_executable(GigaRayV0
    main.cpp
    common.h 
    common.cpp
    defines.h
    obj.h 
    obj.cpp
    renderer.h 
    renderer.cpp
    vk_helpers.h
)

set_property(TARGET GigaRayV0 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

target_include_directories(GigaRayV0 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(GigaRayV0 vma tinyobjloader)
target_link_libraries(GigaRayV0 Vulkan::Vulkan sdl2)
add_dependencies(GigaRayV0 Shaders)