# CMakeList.txt : CMake project for CrossHatchEditor, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("CrossHatchEditor")

set(SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(MESHES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/meshes")
set(NOISE_TEX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/noise textures")

# Generate Shaders

# Add source to this project's executable.
add_executable (${PROJECT_NAME})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME}  PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
target_sources(${PROJECT_NAME} PRIVATE 
"CrossHatchEditor.cpp" 
"CrossHatchEditor.h"
"Camera.h"
"InputManager.cpp"
"InputManager.h" 
"ObjLoader.cpp"
"ObjLoader.h" 
"PrimitiveObjects.h"
"bgfx-imgui/imgui_impl_bgfx.cpp" "Logger.cpp" "Light.h" "stb_image.h" "stb_image_write.h" "VideoPlayer.h")

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

add_subdirectory(./bgfx.cmake)
add_subdirectory(./glfw)

BGFX_COMPILE_SHADERS(
    TYPE FRAGMENT
    SHADERS ${CMAKE_SOURCE_DIR}/baseshaders/crosshatching_frag_variant1.sc
    VARYING_DEF ${CMAKE_SOURCE_DIR}/baseshaders/varying.def.sc
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/include/generated/shaders
    AS_HEADERS
)

add_library(shaderLib INTERFACE)
target_include_directories(shaderLib INTERFACE ${CMAKE_BINARY_DIR}/include/generated/shaders)

target_link_libraries(${PROJECT_NAME}  PRIVATE bgfx bx bimg glfw imgui shaderLib)
target_include_directories(${PROJECT_NAME}  PRIVATE bgfx)

file(COPY ${SHADERS_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${SHADERS_DIR})

file(COPY ${MESHES_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${MESHES_DIR})

file(COPY ${NOISE_TEX_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${NOISE_TEX_DIR})

set(IMGUI_PATH ${CMAKE_CURRENT_LIST_DIR}/imgui)
file(GLOB IMGUI_GLOB
    ${IMGUI_PATH}/imgui.h
    ${IMGUI_PATH}/imgui.cpp
    ${IMGUI_PATH}/imconfig.h
    ${IMGUI_PATH}/imgui_demo.cpp
    ${IMGUI_PATH}/imgui_draw.cpp
    ${IMGUI_PATH}/imgui_internal.h
    ${IMGUI_PATH}/imstb_rectpack.h
    ${IMGUI_PATH}/imstb_textedit.h
    ${IMGUI_PATH}/imstb_truetype.h
    ${IMGUI_PATH}/imgui_tables.cpp
    ${IMGUI_PATH}/imgui_widgets.cpp

    # specific bindings...
    ${IMGUI_PATH}/backends/imgui_impl_glfw.h
    ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.h
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3_loader.cpp)
add_library("imgui" STATIC ${IMGUI_GLOB} "Logger.cpp")
target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
target_link_libraries("imgui" PRIVATE glfw)

target_include_directories(CrossHatchEditor PUBLIC imgui)

# Set the path where Assimp was built
set(ASSIMP_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/assimp)

# Include Assimp headers
target_include_directories(CrossHatchEditor PRIVATE ${ASSIMP_ROOT_DIR}/include)

# Link Assimp library
target_link_directories(CrossHatchEditor PRIVATE ${ASSIMP_ROOT_DIR}/lib)
target_link_libraries(CrossHatchEditor PRIVATE assimp-vc143-mtd)

set(IMGUIZMO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo")
# Add ImGuizmo as a static library.
add_library(ImGuizmo STATIC
    "${IMGUIZMO_PATH}/ImGuizmo.h"
    "${IMGUIZMO_PATH}/ImGuizmo.cpp"
    # Optionally, if you also want the .h in the build, though usually .h is not compiled:
    # "${IMGUIZMO_PATH}/ImGuizmo.h"
)
# Tell ImGuizmo where to find its own headers AND the imgui headers.
target_include_directories(ImGuizmo PUBLIC
    "${IMGUIZMO_PATH}"
    "${IMGUI_PATH}"
)
# Now link ImGuizmo:
target_link_libraries(CrossHatchEditor PRIVATE ImGuizmo)

add_custom_command(TARGET CrossHatchEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_ROOT_DIR}/bin/assimp-vc143-mtd.dll"  # Replace with the actual path to assimp.dll
        $<TARGET_FILE_DIR:CrossHatchEditor>)

# FFmpeg setup
set(FFMPEG_DIR "${CMAKE_CURRENT_LIST_DIR}/ffmpeg")  # Adjust this path
target_include_directories(CrossHatchEditor PRIVATE ${FFMPEG_DIR}/include)
target_link_directories(CrossHatchEditor PRIVATE ${FFMPEG_DIR}/lib)

target_link_libraries(CrossHatchEditor PRIVATE
    avcodec
    avformat
    avutil
    swscale
    swresample
)

# Copy FFmpeg DLLs after build
add_custom_command(TARGET CrossHatchEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DIR}/bin/avcodec-61.dll"
        "${FFMPEG_DIR}/bin/avformat-61.dll"
        "${FFMPEG_DIR}/bin/avutil-59.dll"
        "${FFMPEG_DIR}/bin/swscale-8.dll"
        "${FFMPEG_DIR}/bin/swresample-5.dll"
        $<TARGET_FILE_DIR:CrossHatchEditor>
)

# Packaging

# Set output directory for build binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Install the executable
install(TARGETS CrossHatchEditor
        RUNTIME DESTINATION .)

# Install shader files
install(DIRECTORY ${SHADERS_DIR} DESTINATION .)
install(DIRECTORY ${MESHES_DIR} DESTINATION .)
install(DIRECTORY "${NOISE_TEX_DIR}" DESTINATION .)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/imgui.ini DESTINATION .)

# Install required DLLs
install(FILES
    "${ASSIMP_ROOT_DIR}/bin/assimp-vc143-mtd.dll"
    DESTINATION .
)

include(CPack)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "CrossHatchEditor")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")