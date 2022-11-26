add_executable(GigaRayV0
    main.cpp
    common.h 
    common.cpp
    defines.h
    ecs.h
    ecs.cpp
    game.h 
    game.cpp
    input.h
    input.cpp 
    misc.h 
    misc.cpp
    obj.h 
    obj.cpp
    platform.h
    platform.cpp
    renderer.h 
    renderer.cpp
    resource_manager.h
    resource_manager.cpp
    r_mesh.h 
    r_mesh.cpp
    r_vulkan.h 
    r_vulkan.cpp
    timer.h 
    timer.cpp
    vk_helpers.h
)

set_property(TARGET GigaRayV0 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

target_include_directories(GigaRayV0 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(GigaRayV0 PRIVATE "${PROJECT_SOURCE_DIR}/external")
target_link_libraries(GigaRayV0 vma tinyobjloader glm spirv-reflect)
target_link_libraries(GigaRayV0 Vulkan::Vulkan sdl2)
add_dependencies(GigaRayV0 Shaders)