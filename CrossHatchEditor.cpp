// CrossHatchEditor.cpp : Defines the entry point for the application.
//
#include "CrossHatchEditor.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <random>
#include "InputManager.h"
#include "Camera.h"
#include "PrimitiveObjects.h"

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

bgfx::ShaderHandle loadShader(const char* shaderPath)
{
    std::ifstream file(shaderPath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to load shader: " << shaderPath << std::endl;
        return BGFX_INVALID_HANDLE;
    }

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(fileSize));
    file.read(buffer.data(), fileSize);

    const bgfx::Memory* mem = bgfx::copy(buffer.data(), static_cast<uint32_t>(fileSize));
    //std::cout << "Shader loaded: " << shaderPath << std::endl;
    return bgfx::createShader(mem);
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

    // Load shaders
    bgfx::ShaderHandle vsh = loadShader("shaders\\vs_cel.bin");
    bgfx::ShaderHandle fsh = loadShader("shaders\\fs_cel.bin");
    bgfx::ProgramHandle defaultProgram = bgfx::createProgram(vsh, fsh, true);

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
        .end();

    bgfx::VertexBufferHandle vbh_plane = bgfx::createVertexBuffer(
        bgfx::makeRef(planeVertices, sizeof(planeVertices)),
        layout
    );

    bgfx::IndexBufferHandle ibh_plane = bgfx::createIndexBuffer(
        bgfx::makeRef(planeIndices, sizeof(planeIndices))
    );

	std::vector<PosColorVertex> sphereVertices;
	std::vector<uint16_t> sphereIndices;

	generateSphere(1.0f, 20, 20, sphereVertices, sphereIndices);

	bgfx::VertexBufferHandle vbh_sphere = bgfx::createVertexBuffer(
		bgfx::makeRef(sphereVertices.data(), sizeof(PosColorVertex) * sphereVertices.size()),
		layout
	);

	bgfx::IndexBufferHandle ibh_sphere = bgfx::createIndexBuffer(
		bgfx::makeRef(sphereIndices.data(), sizeof(uint16_t) * sphereIndices.size())
	);


    //Enable debug output
    bgfx::setDebug(BGFX_DEBUG_TEXT); // <-- Add this line here

    bgfx::setViewRect(0, 0, 0, WNDW_WIDTH, WNDW_HEIGHT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    glfwSetKeyCallback(window, glfw_keyCallback);
    InputManager::initialize(window);

    //declare camera instance

    Camera camera;

    //MAIN LOOP
	while (!glfwWindowShouldClose(window))
	{
        glfwPollEvents();

        //handle inputs
        InputManager::update(camera, 0.016f);

        float view[16];
        bx::mtxLookAt(view, camera.position, bx::add(camera.position, camera.front), camera.up);

        float proj[16];
        bx::mtxProj(proj, 60.0f, float(WNDW_WIDTH) / float(WNDW_HEIGHT), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(0, view, proj);

        // Set model matrix
        float mtx[16];
        bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bgfx::setTransform(mtx);

        bgfx::touch(0);
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 1, 0x4f, "Crosshatching Editor Ver 0.1");
        bgfx::dbgTextPrintf(0, 2, 0x4f, "Nothing here yet...");
        bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);

        // Enable stats or debug text
        bgfx::setDebug(s_showStats ? BGFX_DEBUG_STATS : BGFX_DEBUG_TEXT);

        float planeModel[16];
        bx::mtxIdentity(planeModel);
        bgfx::setTransform(planeModel);
        bgfx::setVertexBuffer(0, vbh_plane);
        bgfx::setIndexBuffer(ibh_plane);
        bgfx::submit(0, defaultProgram);

        bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bgfx::setTransform(mtx);

		bgfx::setVertexBuffer(0, vbh_sphere);
		bgfx::setIndexBuffer(ibh_sphere);
		bgfx::submit(0, defaultProgram);


        // End frame
        bgfx::frame();
	}
	bgfx::shutdown();
	glfwDestroyWindow(window);
    bgfx::destroy(vbh_plane);
	bgfx::destroy(ibh_plane);
	bgfx::destroy(vbh_sphere);
	bgfx::destroy(ibh_sphere);
	bgfx::destroy(defaultProgram);
	glfwTerminate();

	return 0;
}
