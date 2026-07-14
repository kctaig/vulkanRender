cmake_minimum_required(VERSION 3.24)
project(VulkanRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VR_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

include(FetchContent)

find_package(Vulkan REQUIRED)
find_package(glm CONFIG QUIET)

if(NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc HINTS ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE})
endif()

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK with glslc.")
endif()

if(NOT TARGET glm::glm)
    message(STATUS "glm::glm target not found, fetching GLM from GitHub...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

set(VR_SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/scene/MeshIO.cpp
    src/renderer/Renderer.cpp
    src/core/VulkanContext.cpp
    src/renderer/ForwardPass.cpp
)

add_executable(vulkan_renderer ${VR_SOURCES})

set(VR_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(VR_SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${VR_SHADER_BINARY_DIR})

set(VR_SHADER_SOURCES
    ${VR_SHADER_SOURCE_DIR}/triangle.vert
    ${VR_SHADER_SOURCE_DIR}/triangle.frag
)

set(VR_SHADER_OUTPUTS)
foreach(SHADER_FILE IN LISTS VR_SHADER_SOURCES)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    set(SPIRV_FILE ${VR_SHADER_BINARY_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.2 ${SHADER_FILE} -o ${SPIRV_FILE}
        DEPENDS ${SHADER_FILE}
        VERBATIM
        COMMENT "Compiling shader ${SHADER_NAME}"
    )

    list(APPEND VR_SHADER_OUTPUTS ${SPIRV_FILE})
endforeach()

add_custom_target(vulkan_shaders ALL DEPENDS ${VR_SHADER_OUTPUTS})
add_dependencies(vulkan_renderer vulkan_shaders)

target_include_directories(vulkan_renderer PRIVATE include src)
target_link_libraries(vulkan_renderer PRIVATE Vulkan::Vulkan glm::glm user32 gdi32 shell32)

target_compile_definitions(vulkan_renderer PRIVATE
    $<$<AND:$<BOOL:${VR_ENABLE_VALIDATION}>,$<CONFIG:Debug>>:VR_ENABLE_VALIDATION=1>
    VR_SHADER_DIR="${VR_SHADER_BINARY_DIR}"
    VR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${VR_SOURCES})
