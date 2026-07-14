cmake_minimum_required(VERSION 3.24)
project(VulkanRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8") 

option(VR_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

find_package(Vulkan REQUIRED)

# --- glslc ---
if(NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc HINTS ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE})
endif()
if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK with glslc.")
endif()

# --- GLM (header-only, vendored in 3dparty) ---
if(NOT TARGET glm::glm)
    set(GLM_DIR ${CMAKE_SOURCE_DIR}/3dparty/glm)
    if(EXISTS ${GLM_DIR}/glm/glm.hpp)
        add_library(glm::glm INTERFACE IMPORTED)
        target_include_directories(glm::glm INTERFACE ${GLM_DIR})
        message(STATUS "GLM: using vendored copy at ${GLM_DIR}")
    else()
        message(FATAL_ERROR "GLM not found at ${GLM_DIR}")
    endif()
endif()

set(VR_SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/scene/MeshIO.cpp
    src/renderer/Renderer.cpp
    src/core/VulkanContext.cpp
    src/renderer/ForwardPass.cpp
)

set(VR_HEADERS
    include/core/Application.h
    include/core/VulkanContext.h
    include/renderer/RenderPass.h
    include/renderer/Renderer.h
    include/renderer/ForwardPass.h
    include/scene/Camera.h
    include/scene/Scene.h
    include/scene/MeshIO.h
)

add_executable(vulkan_renderer ${VR_SOURCES} ${VR_HEADERS})

set(VR_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(VR_SHADER_BINARY_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${VR_SHADER_BINARY_DIR})

file(GLOB VR_SHADER_SOURCES
    ${VR_SHADER_SOURCE_DIR}/*.vert
    ${VR_SHADER_SOURCE_DIR}/*.frag
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
    VR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resource"
)


source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src   FILES ${VR_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include FILES ${VR_HEADERS})
