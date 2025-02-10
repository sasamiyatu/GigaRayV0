cmake_minimum_required (VERSION 3.8)

project ("GigaRayV0")

set(CMAKE_CXX_STANDARD 17)

set(VULKAN_SDK_DIR $ENV{VULKAN_SDK})
set(SDL2_DIR ${VULKAN_SDK_DIR}/cmake)

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(external)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)
add_subdirectory(lightmapper)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/*.rgen"
    "${PROJECT_SOURCE_DIR}/shaders/*.rmiss"
    "${PROJECT_SOURCE_DIR}/shaders/*.rchit"
    "${PROJECT_SOURCE_DIR}/shaders/*.rahit"
    "${PROJECT_SOURCE_DIR}/shaders/*.rint"
    )

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/spirv/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    ##execute glslang command to compile that specific shader
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)