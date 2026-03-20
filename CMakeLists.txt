cmake_minimum_required(VERSION 3.24)
project(VulkanRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VR_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

include(cmake/CompilerWarnings.cmake)
include(FetchContent)

find_package(Vulkan REQUIRED)
find_package(glm CONFIG QUIET)

if(NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc HINTS ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE})
endif()

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK with glslc.")
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan)

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
    src/renderer/Device.cpp
    src/renderer/Swapchain.cpp
    src/renderer/ResourceManager.cpp
    src/renderer/PipelineManager.cpp
    src/renderer/RenderGraph.cpp
    src/renderer/passes/GBufferPass.cpp
    src/renderer/passes/ShadowPass.cpp
    src/renderer/passes/LightingPass.cpp
    src/renderer/passes/PostProcessPass.cpp
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

target_include_directories(vulkan_renderer PRIVATE src)
target_link_libraries(vulkan_renderer PRIVATE Vulkan::Vulkan glm::glm imgui user32 gdi32 shell32)

target_compile_definitions(vulkan_renderer PRIVATE
    $<$<BOOL:${VR_ENABLE_VALIDATION}>:VR_ENABLE_VALIDATION=1>
    VR_SHADER_DIR="${VR_SHADER_BINARY_DIR}"
    VR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

set_project_warnings(vulkan_renderer)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${VR_SOURCES})
