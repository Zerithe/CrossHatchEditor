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

# Add source to this project's executable.
add_executable (CrossHatchEditor 
"CrossHatchEditor.cpp" 
"CrossHatchEditor.h"
"Camera.h"
"InputManager.cpp"
"InputManager.h" 
"ObjLoader.cpp"
"ObjLoader.h" 
"PrimitiveObjects.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CrossHatchEditor PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
add_subdirectory(./bgfx.cmake)
add_subdirectory(./glfw)
target_link_libraries(CrossHatchEditor PRIVATE bgfx bx bimg glfw imgui)
target_include_directories(CrossHatchEditor PRIVATE bgfx)

file(COPY ${SHADERS_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${SHADERS_DIR})


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
add_library("imgui" STATIC ${IMGUI_GLOB})
target_include_directories("imgui" PUBLIC ${IMGUI_PATH})
target_link_libraries("imgui" PRIVATE glfw)

target_include_directories(CrossHatchEditor PUBLIC imgui)

# Set the path where Assimp was built
set(ASSIMP_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/assimp)

# Include Assimp headers
target_include_directories(CrossHatchEditor PRIVATE ${ASSIMP_ROOT_DIR}/include)

# Link Assimp library
target_link_directories(CrossHatchEditor PRIVATE ${ASSIMP_ROOT_DIR}/lib/Debug)
target_link_libraries(CrossHatchEditor PRIVATE assimp-vc143-mtd)

add_custom_command(TARGET CrossHatchEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_ROOT_DIR}/bin/Debug/assimp-vc143-mtd.dll"  # Replace with the actual path to assimp.dll
        $<TARGET_FILE_DIR:CrossHatchEditor>)