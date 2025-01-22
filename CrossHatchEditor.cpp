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
#include "ObjLoader.h"

#include <bgfx/bgfx.h>
#include <bx/uint32_t.h>
#include <bgfx/platform.h>
#include <bx/commandline.h>
#include <bx/endian.h>
#include <bx/math.h>
#include <bx/readerwriter.h>
#include <bx/string.h>

//include embedded shaders

#include <bgfx/defines.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_dx11.h>
#include "bgfx-imgui/imgui_impl_bgfx.h"

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif


#define WNDW_WIDTH 1600
#define WNDW_HEIGHT 900

static bool s_showStats = false;
bgfx::UniformHandle u_lightDir;
bgfx::UniformHandle u_lightColor;
bgfx::UniformHandle u_viewPos;


struct Instance
{
    float position[3];
    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;

    Instance(float x, float y, float z, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh)
        : vertexBuffer(vbh), indexBuffer(ibh)
    {
        position[0] = x;
        position[1] = y;
        position[2] = z;
    }
};

static void glfw_keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_F1 && action == GLFW_RELEASE)
        s_showStats = !s_showStats;
}

float getRandomFloat()
{
    static std::random_device rd;  // Seed for random number engine
    static std::mt19937 gen(rd()); // Mersenne Twister engine
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f); // Distribution range [0.0, 1.0]

    return dist(gen);
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

static void spawnInstance(Camera camera, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, std::vector<Instance>& instances)
{

    float spawnDistance = 5.0f;
    // Position for new instance, e.g., random or predefined position
    float x = camera.position.x + camera.front.x * spawnDistance;
    float y = camera.position.y + camera.front.y * spawnDistance;
    float z = camera.position.z + camera.front.z * spawnDistance;

    // Create a new instance with the current vertex and index buffers
    instances.emplace_back(x, y, z, vbh, ibh);
    std::cout << "New instance created at (" << x << ", " << y << ", " << z << ")" << std::endl;
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

    //Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_Implbgfx_Init(255);

    // Load shaders

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
        .end();

    //plane generation
    bgfx::VertexBufferHandle vbh_plane = bgfx::createVertexBuffer(
        bgfx::makeRef(planeVertices, sizeof(planeVertices)),
        layout
    );

    bgfx::IndexBufferHandle ibh_plane = bgfx::createIndexBuffer(
        bgfx::makeRef(planeIndices, sizeof(planeIndices))
    );

    //cube generation
	bgfx::VertexBufferHandle vbh_cube = bgfx::createVertexBuffer(
		bgfx::makeRef(cubeVertices, sizeof(cubeVertices)),
		layout
	);
	bgfx::IndexBufferHandle ibh_cube = bgfx::createIndexBuffer(
		bgfx::makeRef(cubeIndices, sizeof(cubeIndices))
	);

    //capsule generation
	bgfx::VertexBufferHandle vbh_capsule = bgfx::createVertexBuffer(
		bgfx::makeRef(capsuleVertices, sizeof(capsuleVertices)),
		layout
	);
	bgfx::IndexBufferHandle ibh_capsule = bgfx::createIndexBuffer(
		bgfx::makeRef(capsuleIndices, sizeof(capsuleIndices))
	);

    
	//cylinder generation
	bgfx::VertexBufferHandle vbh_cylinder = bgfx::createVertexBuffer(
		bgfx::makeRef(cylinderVertices, sizeof(cylinderVertices)),
		layout
	);
	bgfx::IndexBufferHandle ibh_cylinder = bgfx::createIndexBuffer(
		bgfx::makeRef(cylinderIndices, sizeof(cylinderIndices))
	);

	
    //sphere generation
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

    // Load OBJ file
    std::vector<ObjLoader::Vertex> suzanneVertices;
    std::vector<uint16_t> suzanneIndices;
    if (!ObjLoader::loadObj("suzanne.obj", suzanneVertices, suzanneIndices)) {
        std::cerr << "Failed to load OBJ file" << std::endl;
        return -1;
    }

    bgfx::VertexBufferHandle suzanneVbh = ObjLoader::createVertexBuffer(suzanneVertices);
    bgfx::IndexBufferHandle suzanneIbh = ObjLoader::createIndexBuffer(suzanneIndices);

    //Enable debug output
    bgfx::setDebug(BGFX_DEBUG_TEXT); // <-- Add this line here

    bgfx::setViewRect(0, 0, 0, WNDW_WIDTH, WNDW_HEIGHT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    glfwSetKeyCallback(window, glfw_keyCallback);
    InputManager::initialize(window);

    //declare camera instance

    Camera camera;

    std::vector<Instance> instances;

    bool modelMovement = true;

    float lightColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    float lightDir[4] = { 0.0f, 1.0f, 1.0f, 0.0f };

    int spawnPrimitive = 0;

    //MAIN LOOP
	while (!glfwWindowShouldClose(window))
	{
        glfwPollEvents();
        
        //imgui loop
		ImGui_ImplGlfw_NewFrame();
		ImGui_Implbgfx_NewFrame();
		ImGui::NewFrame();
		ImGui::ShowDemoWindow();

        //handle inputs
        InputManager::update(camera, 0.016f);

		if (InputManager::isKeyToggled(GLFW_KEY_1))
		{
			spawnPrimitive = 0;
		}
        if (InputManager::isKeyToggled(GLFW_KEY_2))
        {
            spawnPrimitive = 1;
        }
		if (InputManager::isKeyToggled(GLFW_KEY_3))
		{
			spawnPrimitive = 2;
		}
		if (InputManager::isKeyToggled(GLFW_KEY_4))
		{
			spawnPrimitive = 3;
		}
		if (InputManager::isKeyToggled(GLFW_KEY_5))
		{
			spawnPrimitive = 4;
		}
		if (InputManager::isKeyToggled(GLFW_KEY_6))
		{
			spawnPrimitive = 5;
		}

        if (InputManager::isKeyToggled(GLFW_KEY_M))
        {
            modelMovement = !modelMovement;
            std::cout << "Model movement: " << modelMovement << std::endl;
        }

        //call spawnInstance when key is pressed based on spawnPrimitive value
		if (InputManager::isMouseClicked(GLFW_MOUSE_BUTTON_LEFT))
		{
			if (spawnPrimitive == 0)
			{
				spawnInstance(camera, vbh_cube, ibh_cube, instances);
				std::cout << "Cube spawned" << std::endl;
			}
			else if (spawnPrimitive == 1)
			{
				spawnInstance(camera, vbh_capsule, ibh_capsule, instances);
				std::cout << "Capsule spawned" << std::endl;
			}
			else if (spawnPrimitive == 2)
			{
				spawnInstance(camera, vbh_cylinder, ibh_cylinder, instances);
				std::cout << "Cylinder spawned" << std::endl;
			}
			else if (spawnPrimitive == 3)
			{
				spawnInstance(camera, vbh_sphere, ibh_sphere, instances);
				std::cout << "Sphere spawned" << std::endl;
			}
			else if (spawnPrimitive == 4)
			{
				spawnInstance(camera, vbh_plane, ibh_plane, instances);
				std::cout << "Plane spawned" << std::endl;
			}
            else if (spawnPrimitive == 5)
            {
				spawnInstance(camera, suzanneVbh, suzanneIbh, instances);
				std::cout << "Suzanne spawned" << std::endl;
            }
		}

        if (InputManager::isKeyToggled(GLFW_KEY_BACKSPACE) && !instances.empty())
        {
            instances.pop_back();
            std::cout << "Last Instance removed" << std::endl;
        }

        if(InputManager::isKeyToggled(GLFW_KEY_C))
        {
            lightColor[0] = getRandomFloat(); // Random red
            lightColor[1] = getRandomFloat(); // Random green
            lightColor[2] = getRandomFloat(); // Random blue
            lightColor[3] = 1.0f;

        }

        if (InputManager::isKeyToggled(GLFW_KEY_X))
        {
            lightColor[0] = 0.5f;
            lightColor[1] = 0.5f;
            lightColor[2] = 0.5f;
            lightColor[3] = 1.0f;
        }

        if (InputManager::isKeyToggled(GLFW_KEY_V))
        {
            lightDir[0] = getRandomFloat();
            lightDir[1] = getRandomFloat();
            lightDir[2] = getRandomFloat();
            lightDir[3] = 0.0f;
        }

        if (InputManager::isKeyToggled(GLFW_KEY_Z))
        {
            lightColor[0] = 0.0f;
            lightColor[1] = 1.0f;
            lightColor[2] = 1.0f;
            lightColor[3] = 0.0f;
        }

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

        float viewPos[4] = { camera.position.x, camera.position.y, camera.position.z, 1.0f };

        bgfx::setUniform(u_lightDir, lightDir);
        bgfx::setUniform(u_lightColor, lightColor);
        bgfx::setUniform(u_viewPos, viewPos);

        // Create uniform handles for the light direction and color
        u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
        u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
        u_viewPos = bgfx::createUniform("u_viewPos", bgfx::UniformType::Vec4);

        bgfx::UniformHandle u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);

        // Example parameter values
        float params[4] = { 0.02f, 15.0f, 0.0f, 0.0f }; // e = 0.02 (tolerance), scale = 15.0
        bgfx::setUniform(u_params, params);



        bgfx::ShaderHandle vsh = loadShader("shaders\\vs_cel.bin");
        bgfx::ShaderHandle fsh = loadShader("shaders\\crosshatching_frag_variant1.bin");
        bgfx::ProgramHandle defaultProgram = bgfx::createProgram(vsh, fsh, true);


        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 1, 0x4f, "Crosshatching Editor Ver 0.1");
        bgfx::dbgTextPrintf(0, 2, 0x4f, "Nothing here yet...");
        bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);
        bgfx::dbgTextPrintf(0, 4, 0x0f, "M - Toggle Object Movement");
        bgfx::dbgTextPrintf(0, 5, 0x0f, "C - Randomize Light Color");
        bgfx::dbgTextPrintf(0, 6, 0x0f, "X - Reset Light Color");
        bgfx::dbgTextPrintf(0, 7, 0x0f, "V - Randomize Light Direction");
        bgfx::dbgTextPrintf(0, 8, 0x0f, "Z - Reset Light Direction");
        bgfx::dbgTextPrintf(0, 9, 0x0f, "F1 - Toggle stats");

        // Enable stats or debug text
        bgfx::setDebug(s_showStats ? BGFX_DEBUG_STATS : BGFX_DEBUG_TEXT);

        float planeModel[16];
        bx::mtxIdentity(planeModel);
        bgfx::setTransform(planeModel);
        bgfx::setVertexBuffer(0, vbh_plane);
        bgfx::setIndexBuffer(ibh_plane);
        bgfx::submit(0, defaultProgram);

        if (modelMovement)
        {
            for (const auto& instance : instances)
            {
                float model[16];
                // Calculate the forward vector pointing towards the camera
                bx::Vec3 modelToCamera = bx::normalize(bx::Vec3(camera.position.x - instance.position[0],
                    camera.position.y - instance.position[1],
                    camera.position.z - instance.position[2]));


                // Define the up vector
                bx::Vec3 up = { 0.0f, 1.0f, 0.0f };

                // Calculate the right vector (orthogonal to forward and up)
                bx::Vec3 right = bx::normalize(bx::cross(up, modelToCamera));

                // Recalculate the up vector to ensure orthogonality
                up = bx::normalize(bx::cross(modelToCamera, right));

                // Build the model matrix with the orientation facing the camera
                model[0] = right.x;   model[1] = right.y;   model[2] = right.z;   model[3] = 0.0f;
                model[4] = up.x;      model[5] = up.y;      model[6] = up.z;      model[7] = 0.0f;
                model[8] = modelToCamera.x; model[9] = modelToCamera.y; model[10] = modelToCamera.z; model[11] = 0.0f;
                model[12] = instance.position[0];
                model[13] = instance.position[1];
                model[14] = instance.position[2];
                model[15] = 1.0f;
                bgfx::setTransform(model);

                bgfx::setVertexBuffer(0, instance.vertexBuffer);
                bgfx::setIndexBuffer(instance.indexBuffer);
                bgfx::submit(0, defaultProgram);
            }
        }
        else
        {
            for (const auto& instance : instances)
            {
                float model[16];
                bx::mtxTranslate(model, instance.position[0], instance.position[1], instance.position[2]);
                bgfx::setTransform(model);

                bgfx::setVertexBuffer(0, instance.vertexBuffer);
                bgfx::setIndexBuffer(instance.indexBuffer);
                bgfx::submit(0, defaultProgram);
            }
        }

        // Update your vertex layout to include normals
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .end();



        bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bgfx::setTransform(mtx);

		bgfx::setVertexBuffer(0, vbh_sphere);
		bgfx::setIndexBuffer(ibh_sphere);
		bgfx::submit(0, defaultProgram);


        // End frame
		ImGui::Render();
		ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());
        bgfx::frame();
	}
    for (const auto& instance : instances)
    {
        bgfx::destroy(instance.vertexBuffer);
        bgfx::destroy(instance.indexBuffer);
    }

    bgfx::destroy(vbh_plane);
	bgfx::destroy(ibh_plane);
	bgfx::destroy(vbh_sphere);
	bgfx::destroy(ibh_sphere);
    bgfx::destroy(u_lightColor);
	bgfx::destroy(u_lightDir);
	bgfx::destroy(u_viewPos);
	bgfx::destroy(vbh_cube);
	bgfx::destroy(ibh_cube);
	bgfx::destroy(vbh_capsule);
	bgfx::destroy(ibh_capsule);
	bgfx::destroy(vbh_cylinder);
	bgfx::destroy(ibh_cylinder);
	//bgfx::destroy(defaultProgram);
    ImGui_ImplGlfw_Shutdown();
	
    bgfx::shutdown();
    glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
