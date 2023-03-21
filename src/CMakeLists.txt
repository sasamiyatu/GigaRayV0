add_executable(GigaRayV0
    main.cpp
    brdf.h
    common.h 
    common.cpp
    defines.h
    ecs.h
    ecs.cpp
    events.h
    events.cpp
    g_math.h
    g_math.cpp
    game.h 
    game.cpp
    gbuffer.h 
    gbuffer.cpp
    gltf.h 
    gltf.cpp
    input.h
    input.cpp
    janitor.h
    janitor.cpp
    lightmap.h
    lightmap.cpp
    logging.h
    logging.cpp
    material.h 
    material.cpp
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
    sampling.h
    sampling.cpp
    shaders.h 
    shaders.cpp
    timer.h 
    timer.cpp
    vk_helpers.h
    imgui/imconfig.h
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_sdl2.cpp
    imgui/imgui_impl_sdl2.h
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_vulkan.h
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
)

set_property(TARGET GigaRayV0 PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_compile_definitions(VK_NO_PROTOTYPES)
target_include_directories(GigaRayV0 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(GigaRayV0 PRIVATE "${PROJECT_SOURCE_DIR}/external")
target_include_directories(GigaRayV0 PRIVATE ${Vulkan_INCLUDE_DIRS}) 
target_link_libraries(GigaRayV0 vma tinyobjloader glm spirv-reflect xatlas)
target_link_libraries(GigaRayV0 Vulkan::Vulkan sdl2)
add_dependencies(GigaRayV0 Shaders)