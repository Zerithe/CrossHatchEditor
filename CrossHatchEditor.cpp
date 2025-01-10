// CrossHatchEditor.cpp : Defines the entry point for the application.
//

#include "CrossHatchEditor.h"
#include <iostream>
#include <bgfx/bgfx.h>
#include <bx/uint32_t.h>
#include <bgfx/platform.h>
#include <bx/commandline.h>
#include <bx/endian.h>
#include <bx/math.h>
#include <bx/readerwriter.h>
#include <bx/string.h>

#include <bgfx/defines.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>


#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif


#define WNDW_WIDTH 1600
#define WNDW_HEIGHT 900

static bool s_showStats = false;

static void glfw_keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_F1 && action == GLFW_RELEASE)
        s_showStats = !s_showStats;
}

void main_loop() 
{
    glfwPollEvents();
	bgfx::touch(0);
	bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 1, 0x4f, "Crosshatching Editor Ver X.X");
    bgfx::dbgTextPrintf(0, 2, 0x4f, "Nothing here yet...");
    bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);

    // Enable stats or debug text
    bgfx::setDebug(s_showStats ? BGFX_DEBUG_STATS : BGFX_DEBUG_TEXT);


    // End frame
    bgfx::frame();
}

int main(void)
{
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    GLFWwindow* window = glfwCreateWindow(WNDW_WIDTH, WNDW_HEIGHT, "CrossHatchEditor", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    // Initialize BGFX
    bgfx::renderFrame();

    bgfx::Init bgfxinit;
    bgfxinit.type = bgfx::RendererType::OpenGL;
    bgfxinit.resolution.width = WNDW_WIDTH;
    bgfxinit.resolution.height = WNDW_HEIGHT;
    bgfxinit.resolution.reset = BGFX_RESET_VSYNC;
    bgfxinit.platformData.nwh = glfwGetWin32Window(window);
    if (!bgfx::init(bgfxinit)) {
        std::cerr << "Failed to initialize BGFX" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    //Enable debug output
    bgfx::setDebug(BGFX_DEBUG_TEXT); // <-- Add this line here

    bgfx::setViewRect(0, 0, 0, WNDW_WIDTH, WNDW_HEIGHT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    glfwSetKeyCallback(window, glfw_keyCallback);
	while (!glfwWindowShouldClose(window))
	{
		main_loop();
	}
	bgfx::shutdown();
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
