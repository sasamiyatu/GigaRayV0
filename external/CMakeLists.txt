find_package(Vulkan REQUIRED)

add_library(glm INTERFACE)
add_library(vma INTERFACE)
add_library(stb INTERFACE)
add_library(cgltf INTERFACE)

add_library(tinyobjloader STATIC)
add_library(spirv-reflect STATIC)
add_library(xatlas STATIC)

target_include_directories(vma INTERFACE vma/include)
target_include_directories(glm INTERFACE glm)
target_include_directories(stb INTERFACE stb)
target_include_directories(cgltf INTERFACE cgltf)

add_library(sdl2 INTERFACE)
set(sdl2_DIR "SDL_PATH" CACHE PATH "Path to SDL2")

if (WIN32)
target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include ${sdl2_image_DIR}/include)
target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib/x64 ${sdl2_image_DIR}/lib/x64)
elseif (UNIX)
target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include/SDL2 ${sdl2_image_DIR}/include/SDL2)
target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib ${sdl2_image_DIR}/lib)
endif()

target_sources(tinyobjloader PRIVATE 
    tinyobjloader/tiny_obj_loader.h
    tinyobjloader/tiny_obj_loader.cc
    )

target_sources(spirv-reflect PRIVATE
    spirv-reflect/spirv_reflect.h
    spirv-reflect/spirv_reflect.c
)

target_sources(xatlas PRIVATE
    xatlas/source/xatlas/xatlas.h
    xatlas/source/xatlas/xatlas.cpp
)

target_include_directories(spirv-reflect PUBLIC spirv_reflect)
target_include_directories(tinyobjloader PUBLIC tinyobjloader)
target_include_directories(xatlas PUBLIC xatlas/source/xatlas)

target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

#add_library(imgui SHARED)

#target_include_directories(imgui PUBLIC imgui)

#target_sources(imgui PRIVATE
#    "imgui/imgui.h"
#    "imgui/imgui.cpp"
#
#    "imgui/imgui_demo.cpp"
#    "imgui/imgui_draw.cpp"
#    "imgui/imgui_widgets.cpp"
#    "imgui/imgui_tables.cpp"
#
#    "imgui/backends/imgui_impl_vulkan.cpp"
#    "imgui/backends/imgui_impl_sdl2.cpp"
#)
#
#target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)