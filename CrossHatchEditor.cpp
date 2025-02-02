// CrossHatchEditor.cpp : Defines the entry point for the application.
//
#include "CrossHatchEditor.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <random>
#include "InputManager.h"
#include "Camera.h"
#include "PrimitiveObjects.h"
#include "ObjLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
//bgfx::UniformHandle u_scale;


struct Instance
{
    int id;
    std::string name;
    float position[3];
    float rotation[3]; // Euler angles in radians (for X, Y, Z)
    float scale[3];    // Non-uniform scale for each axis
    bgfx::VertexBufferHandle vertexBuffer;
    bgfx::IndexBufferHandle indexBuffer;
    bool selected = false;

    Instance(int instanceId, const std::string& instanceName, float x, float y, float z, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh)
        : id(instanceId), name(instanceName), vertexBuffer(vbh), indexBuffer(ibh)
    {
        position[0] = x;
        position[1] = y;
        position[2] = z;
        // Initialize with no rotation and uniform scale of 1
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        scale[0] = scale[1] = scale[2] = 1.0f;
    }
};

struct MeshData {
    std::vector<PosColorVertex> vertices;
    std::vector<uint16_t> indices;
};

struct Vec3 {
    float x, y, z;
};

// Moves the instance by (dx, dy, dz)
void translateInstance(Instance& instance, float dx, float dy, float dz)
{
    instance.position[0] += dx;
    instance.position[1] += dy;
    instance.position[2] += dz;
}

// Rotates the instance by the given delta angles (in radians)
void rotateInstance(Instance& instance, float dAngleX, float dAngleY, float dAngleZ)
{
    instance.rotation[0] += dAngleX;
    instance.rotation[1] += dAngleY;
    instance.rotation[2] += dAngleZ;
}

// Scales the instance by the given factors (multiplicatively)
void scaleInstance(Instance& instance, float factorX, float factorY, float factorZ)
{
    instance.scale[0] *= factorX;
    instance.scale[1] *= factorY;
    instance.scale[2] *= factorZ;
}

//transfer to ObjLoader.cpp
void computeNormals(std::vector<PosColorVertex>& vertices, const std::vector<uint16_t>& indices) {

    // Reset all normals to zero
    for (auto& vertex : vertices) {
        vertex.nx = 0.0f;
        vertex.ny = 0.0f;
        vertex.nz = 0.0f;
    }
    // Create an array to accumulate normals
    std::vector<Vec3> accumulatedNormals(vertices.size(), { 0.0f, 0.0f, 0.0f });

    for (size_t i = 0; i < indices.size(); i += 3) {
        uint16_t i0 = indices[i];
        uint16_t i1 = indices[i + 1];
        uint16_t i2 = indices[i + 2];

        Vec3 v0 = { vertices[i0].x, vertices[i0].y, vertices[i0].z };
        Vec3 v1 = { vertices[i1].x, vertices[i1].y, vertices[i1].z };
        Vec3 v2 = { vertices[i2].x, vertices[i2].y, vertices[i2].z };

        Vec3 edge1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
        Vec3 edge2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };

        Vec3 normal = {
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };

        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0f) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }


        // Accumulate the normal for each vertex in the face
        accumulatedNormals[i0].x += normal.x; accumulatedNormals[i0].y += normal.y; accumulatedNormals[i0].z += normal.z;
        accumulatedNormals[i1].x += normal.x; accumulatedNormals[i1].y += normal.y; accumulatedNormals[i1].z += normal.z;
        accumulatedNormals[i2].x += normal.x; accumulatedNormals[i2].y += normal.y; accumulatedNormals[i2].z += normal.z;
    }

    // Normalize the accumulated normals for each vertex
    for (size_t i = 0; i < vertices.size(); i++) {
        Vec3& normal = accumulatedNormals[i];
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0f) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }

        // Assign the normalized normal to the vertex
        vertices[i].nx = normal.x;
        vertices[i].ny = normal.y;
        vertices[i].nz = normal.z;
    }
}

MeshData loadMesh(const std::string& filePath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Error: Assimp - " << importer.GetErrorString() << std::endl;
        return {};
    }

    std::vector<PosColorVertex> vertices;
    std::vector<uint16_t> indices;

    // Iterate through all meshes in the scene
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        size_t baseIndex = vertices.size();
        std::unordered_map<std::string, uint16_t> uniqueVertices;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            PosColorVertex vertex;
            vertex.x = mesh->mVertices[i].x;
            vertex.y = mesh->mVertices[i].y;
            vertex.z = mesh->mVertices[i].z;

            // Retrieve normals if available
            if (mesh->HasNormals()) {
                vertex.nx = mesh->mNormals[i].x;
                vertex.ny = mesh->mNormals[i].y;
                vertex.nz = mesh->mNormals[i].z;
            }
            else {
                vertex.nx = 0.0f;
                vertex.ny = 0.0f;
                vertex.nz = 0.0f; // Default to zero, will recompute later
            }

            if (mesh->HasVertexColors(0)) {
                vertex.abgr = ((uint8_t)(mesh->mColors[0][i].r * 255) << 24) |
                    ((uint8_t)(mesh->mColors[0][i].g * 255) << 16) |
                    ((uint8_t)(mesh->mColors[0][i].b * 255) << 8) |
                    (uint8_t)(mesh->mColors[0][i].a * 255);
            }
            else {
                vertex.abgr = 0xffffffff; // Default color
            }

            /*std::ostringstream key;

            key << vertex.x << "," << vertex.y << "," << vertex.z;

            if (uniqueVertices.count(key.str()) == 0) {
                uniqueVertices[key.str()] = static_cast<uint16_t>(vertices.size());
                vertices.push_back(vertex);
            }*/

            vertices.push_back(vertex);
        }


        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            // Reverse the winding order
            for (int j = face.mNumIndices - 1; j >= 0; j--) {
                indices.push_back(baseIndex + face.mIndices[j]);
            }
        }

        // Compute normals if the mesh does not have them
        if (!mesh->HasNormals()) {
            computeNormals(vertices, indices);
        }
    }

    return { vertices, indices };
}

void createMeshBuffers(const MeshData& meshData, bgfx::VertexBufferHandle& vbh, bgfx::IndexBufferHandle& ibh) {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
        .end();

    vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(meshData.vertices.data(), sizeof(PosColorVertex) * meshData.vertices.size()),
        layout
    );

    ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(meshData.indices.data(), sizeof(uint16_t) * meshData.indices.size())
    );
}
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

static int instanceCounter = 0;

static void spawnInstance(Camera camera, const std::string& instanceName, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, std::vector<Instance>& instances)
{

    float spawnDistance = 5.0f;
    // Position for new instance, e.g., random or predefined position
    float x = camera.position.x + camera.front.x * spawnDistance;
    float y = camera.position.y + camera.front.y * spawnDistance;
    float z = camera.position.z + camera.front.z * spawnDistance;

	std::string fullName = instanceName + std::to_string(instanceCounter);

    // Create a new instance with the current vertex and index buffers
    instances.emplace_back(instanceCounter++, fullName, x, y, z, vbh, ibh);
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
	/*ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_MenuBar;*/
    //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_Implbgfx_Init(255);

    // Load shaders

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
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
	std::vector<PosColorVertex> capsuleVertices;
	std::vector<uint16_t> capsuleIndices;

	generateCapsule(1.0f, 1.5f, 20, 20, capsuleVertices, capsuleIndices);

	bgfx::VertexBufferHandle vbh_capsule = bgfx::createVertexBuffer(
		bgfx::makeRef(capsuleVertices.data(), sizeof(PosColorVertex) * capsuleVertices.size()),
		layout
	);

	bgfx::IndexBufferHandle ibh_capsule = bgfx::createIndexBuffer(
		bgfx::makeRef(capsuleIndices.data(), sizeof(uint16_t) * capsuleIndices.size())
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

    //mesh generation
    MeshData meshData = loadMesh("meshes/cornell-box.obj");
    bgfx::VertexBufferHandle vbh_mesh;
    bgfx::IndexBufferHandle ibh_mesh;
    createMeshBuffers(meshData, vbh_mesh, ibh_mesh);
    //Load OBJ file
 /*   std::vector<ObjLoader::Vertex> suzanneVertices;
    std::vector<uint16_t> suzanneIndices;
    if (!ObjLoader::loadObj("suzanne.obj", suzanneVertices, suzanneIndices)) {
        std::cerr << "Failed to load OBJ file" << std::endl;
        return -1;
    }

    bgfx::VertexBufferHandle vbh_mesh = ObjLoader::createVertexBuffer(suzanneVertices);
    bgfx::IndexBufferHandle ibh_mesh = ObjLoader::createIndexBuffer(suzanneIndices);*/

    //Enable debug output
    bgfx::setDebug(BGFX_DEBUG_TEXT); // <-- Add this line here

    bgfx::setViewRect(0, 0, 0, WNDW_WIDTH, WNDW_HEIGHT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    glfwSetKeyCallback(window, glfw_keyCallback);
    InputManager::initialize(window);

    //declare camera instance

    Camera camera;

    std::vector<Instance> instances;

    bool modelMovement = false;

    float lightColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    float lightDir[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    float scale[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    int spawnPrimitive = 0;

	bool* p_open = NULL;

    //MAIN LOOP
	while (!glfwWindowShouldClose(window))
	{
        glfwPollEvents();
        
        //imgui loop
		ImGui_ImplGlfw_NewFrame();
		ImGui_Implbgfx_NewFrame();
		ImGui::NewFrame();
		//ImGui::ShowDemoWindow();

        /*bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 1, 0x4f, "Crosshatching Editor Ver 0.1");
        bgfx::dbgTextPrintf(0, 2, 0x4f, "Nothing here yet...");
        bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);
        bgfx::dbgTextPrintf(0, 4, 0x0f, "M - Toggle Object Movement");
        bgfx::dbgTextPrintf(0, 5, 0x0f, "C - Randomize Light Color");
        bgfx::dbgTextPrintf(0, 6, 0x0f, "X - Reset Light Color");
        bgfx::dbgTextPrintf(0, 7, 0x0f, "V - Randomize Light Direction");
        bgfx::dbgTextPrintf(0, 8, 0x0f, "Z - Reset Light Direction");
        bgfx::dbgTextPrintf(0, 9, 0x0f, "F1 - Toggle stats");*/


        //IMGUI WINDOW FOR CONTROLS
        //FOR REFERENCE USE THIS: https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
		ImGui::Begin("CrossHatchEditor", p_open, ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open..")) { /* Do stuff */ }
                if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
                if (ImGui::MenuItem("Close", "Ctrl+W")) { /* Do stuff */ }
                ImGui::EndMenu();
            }
			ImGui::EndMenuBar();
		}
		ImGui::Text("Crosshatch Editor Ver 0.2");
		ImGui::Text("Some things here and there...");
		ImGui::Text("Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);
        ImGui::Text("Right Click - Detach Mouse from Camera");
		ImGui::Text("M - Toggle Object Movement");
		ImGui::Text("C - Randomize Light Color");
		ImGui::Text("X - Reset Light Color");
		ImGui::Text("V - Randomize Light Direction");
		ImGui::Text("Z - Reset Light Direction");
		ImGui::Text("F1 - Toggle stats");

		ImGui::Checkbox("Toggle Object Movement", &modelMovement);
        
        if (ImGui::CollapsingHeader("Spawn Objects"))
        {
			ImGui::RadioButton("Cube", &spawnPrimitive, 0);
			ImGui::RadioButton("Capsule", &spawnPrimitive, 1);
			ImGui::RadioButton("Cylinder", &spawnPrimitive, 2);
			ImGui::RadioButton("Sphere", &spawnPrimitive, 3);
			ImGui::RadioButton("Plane", &spawnPrimitive, 4);
			ImGui::RadioButton("Test Import", &spawnPrimitive, 5);
			if (ImGui::Button("Spawn Object"))
			{
				if (spawnPrimitive == 0)
				{
					spawnInstance(camera, "cube", vbh_cube, ibh_cube, instances);
					std::cout << "Cube spawned" << std::endl;
				}
				else if (spawnPrimitive == 1)
				{
					spawnInstance(camera, "capsule", vbh_capsule, ibh_capsule, instances);
					std::cout << "Capsule spawned" << std::endl;
				}
				else if (spawnPrimitive == 2)
				{
					spawnInstance(camera, "cylinder", vbh_cylinder, ibh_cylinder, instances);
					std::cout << "Cylinder spawned" << std::endl;
				}
				else if (spawnPrimitive == 3)
				{
					spawnInstance(camera, "sphere", vbh_sphere, ibh_sphere, instances);
					std::cout << "Sphere spawned" << std::endl;
				}
				else if (spawnPrimitive == 4)
				{
					spawnInstance(camera, "plane", vbh_plane, ibh_plane, instances);
					std::cout << "Plane spawned" << std::endl;
				}
				else if (spawnPrimitive == 5)
				{
					spawnInstance(camera, "mesh", vbh_mesh, ibh_mesh, instances);
					std::cout << "Test Import spawned" << std::endl;
				}
			}
			if (ImGui::Button("Remove Last Instance"))
			{
				instances.pop_back();
                //instanceCounter--;
				std::cout << "Last Instance removed" << std::endl;
			}
        }
		if (ImGui::CollapsingHeader("Lighting"))
		{
			ImGui::ColorEdit3("Light Color", lightColor);
            if (ImGui::Button("Randomize Light Color"))
            {
                lightColor[0] = getRandomFloat(); // Random red
                lightColor[1] = getRandomFloat(); // Random green
                lightColor[2] = getRandomFloat(); // Random blue
                lightColor[3] = 1.0f;
            }
			//lightdir is drag float3
			ImGui::DragFloat3("Light Direction", lightDir, 0.01f, 0.0f, 0.0f);
			//randomize light direction
			if (ImGui::Button("Randomize Light Direction"))
			{
				lightDir[0] = getRandomFloat();
				lightDir[1] = getRandomFloat();
				lightDir[2] = getRandomFloat();
				lightDir[3] = 0.0f;
			}
		}
		if (ImGui::CollapsingHeader("Hierarchy"))
		{
			for (auto& instance : instances)
			{
				const char* instanceName = instance.name.c_str();
				if (ImGui::Selectable(instanceName, instance.selected))
				{
					instance.selected = !instance.selected;
				}
			}
		}
		ImGui::End();

        // A simple panel to select an instance and modify its transform
        static int selectedInstanceIndex = -1;
        ImGui::Begin("Transform Controls");

        // List all instances so the user can select one
        for (int i = 0; i < instances.size(); i++)
        {
            char label[32];
            sprintf(label, "Instance %d", i);
            if (ImGui::Selectable(label, selectedInstanceIndex == i))
            {
                selectedInstanceIndex = i;
            }
        }

        // If an instance is selected, show drag controls for its transform
        if (selectedInstanceIndex >= 0 && selectedInstanceIndex < instances.size())
        {
            Instance& inst = instances[selectedInstanceIndex];
            ImGui::DragFloat3("Translation", inst.position, 0.1f);
            ImGui::DragFloat3("Rotation (radians)", inst.rotation, 0.1f);
            ImGui::DragFloat3("Scale", inst.scale, 0.1f);
        }
        ImGui::End();

        //handle inputs
        InputManager::update(camera, 0.016f);

        if (InputManager::isKeyToggled(GLFW_KEY_M))
        {
            modelMovement = !modelMovement;
            std::cout << "Model movement: " << modelMovement << std::endl;
        }

        //call spawnInstance when key is pressed based on spawnPrimitive value
		if (InputManager::isMouseClicked(GLFW_MOUSE_BUTTON_LEFT) && InputManager::getCursorDisabled())
		{
            if (spawnPrimitive == 0)
            {
                spawnInstance(camera, "cube", vbh_cube, ibh_cube, instances);
                std::cout << "Cube spawned" << std::endl;
            }
            else if (spawnPrimitive == 1)
            {
                spawnInstance(camera, "capsule", vbh_capsule, ibh_capsule, instances);
                std::cout << "Capsule spawned" << std::endl;
            }
            else if (spawnPrimitive == 2)
            {
                spawnInstance(camera, "cylinder", vbh_cylinder, ibh_cylinder, instances);
                std::cout << "Cylinder spawned" << std::endl;
            }
            else if (spawnPrimitive == 3)
            {
                spawnInstance(camera, "sphere", vbh_sphere, ibh_sphere, instances);
                std::cout << "Sphere spawned" << std::endl;
            }
            else if (spawnPrimitive == 4)
            {
                spawnInstance(camera, "plane", vbh_plane, ibh_plane, instances);
                std::cout << "Plane spawned" << std::endl;
            }
            else if (spawnPrimitive == 5)
            {
                spawnInstance(camera, "mesh", vbh_mesh, ibh_mesh, instances);
                std::cout << "Test Import spawned" << std::endl;
            }
		}

        if (InputManager::isKeyToggled(GLFW_KEY_BACKSPACE) && !instances.empty())
        {
            instances.pop_back();
            //instanceCounter--;
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
        //bgfx::setUniform(u_scale, scale);

        // Create uniform handles for the light direction and color
        u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
        u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
        u_viewPos = bgfx::createUniform("u_viewPos", bgfx::UniformType::Vec4);
		//u_scale = bgfx::createUniform("u_scale", bgfx::UniformType::Vec4);

        bgfx::UniformHandle u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);

        // Example parameter values
        float params[4] = { 0.02f, 15.0f, 0.0f, 0.0f }; // e = 0.02 (tolerance), scale = 15.0
        bgfx::setUniform(u_params, params);



        bgfx::ShaderHandle vsh = loadShader("shaders\\vs_cel.bin");
        bgfx::ShaderHandle fsh = loadShader("shaders\\crosshatching_frag_variant1.bin");
        bgfx::ProgramHandle defaultProgram = bgfx::createProgram(vsh, fsh, true);


        /*bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 1, 0x4f, "Crosshatching Editor Ver 0.1");
        bgfx::dbgTextPrintf(0, 2, 0x4f, "Nothing here yet...");
        bgfx::dbgTextPrintf(0, 3, 0x0f, "Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);
        bgfx::dbgTextPrintf(0, 4, 0x0f, "M - Toggle Object Movement");
        bgfx::dbgTextPrintf(0, 5, 0x0f, "C - Randomize Light Color");
        bgfx::dbgTextPrintf(0, 6, 0x0f, "X - Reset Light Color");
        bgfx::dbgTextPrintf(0, 7, 0x0f, "V - Randomize Light Direction");
        bgfx::dbgTextPrintf(0, 8, 0x0f, "Z - Reset Light Direction");
        bgfx::dbgTextPrintf(0, 9, 0x0f, "F1 - Toggle stats");*/

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
                //bx::mtxTranslate(model, instance.position[0], instance.position[1], instance.position[2]);

                // Build a Scale-Rotate-Translate matrix.
                // Note: bx::mtxSRT expects parameters in the order:
                // (result, scaleX, scaleY, scaleZ, rotX, rotY, rotZ, transX, transY, transZ)
                bx::mtxSRT(model,
                    instance.scale[0], instance.scale[1], instance.scale[2],
                    instance.rotation[0], instance.rotation[1], instance.rotation[2],
                    instance.position[0], instance.position[1], instance.position[2]);

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

		/*bgfx::setVertexBuffer(0, vbh_sphere);
		bgfx::setIndexBuffer(ibh_sphere);
		bgfx::submit(0, defaultProgram);*/


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
    bgfx::destroy(vbh_mesh);
    bgfx::destroy(ibh_mesh);
	//bgfx::destroy(defaultProgram);
    ImGui_ImplGlfw_Shutdown();
	
    bgfx::shutdown();
    glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
