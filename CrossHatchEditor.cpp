// CrossHatchEditor.cpp : Defines the entry point for the application.
//
#include "CrossHatchEditor.h"
#include <iostream>
#include <cstring>
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
#include <windows.h>
#include <commdlg.h>
#endif
#include <imgui_internal.h>
#include "Logger.h"


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

    // Add an override object color (RGBA)
    float objectColor[4];
    // NEW: optional diffuse texture for the object.
    bgfx::TextureHandle diffuseTexture = BGFX_INVALID_HANDLE;

    std::vector<Instance*> children; // Hierarchy: child instances
    Instance* parent = nullptr;      // pointer to parent


    Instance(int instanceId, const std::string& instanceName, float x, float y, float z, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh)
        : id(instanceId), name(instanceName), vertexBuffer(vbh), indexBuffer(ibh)
    {
        position[0] = x;
        position[1] = y;
        position[2] = z;
        // Initialize with no rotation and uniform scale of 1
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        scale[0] = scale[1] = scale[2] = 1.0f;
        // Initialize the object color to white (no override)
        objectColor[0] = 1.0f; objectColor[1] = 1.0f; objectColor[2] = 1.0f; objectColor[3] = 1.0f;
    }
    void addChild(Instance* child) {
        children.push_back(child);
        child->parent = this;
    }
};
static Instance* selectedInstance = nullptr;
struct TextureOption {
    std::string name;
    bgfx::TextureHandle handle;
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

std::string openFileDialog(bool save) {
#ifdef _WIN32
    char filePath[MAX_PATH] = { 0 };

    OPENFILENAMEA ofn; // Windows File Picker Struct
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (save) {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn))
            return std::string(filePath);
    }
    else {
        if (GetOpenFileNameA(&ofn))
            return std::string(filePath);
    }
#endif
    return "";
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

            // Retrieve texture coordinates (UVs) if available.
            if (mesh->HasTextureCoords(0)) {
                // Assimp stores texture coordinates in a 3D vector; we use only x and y.
                vertex.u = mesh->mTextureCoords[0][i].x;
                vertex.v = mesh->mTextureCoords[0][i].y;
            }
            else {
                vertex.u = 0.0f;
                vertex.v = 0.0f;
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
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float) // NEW: UV coordinates
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

const bgfx::Memory* loadMem(const char* _filePath)
{
    std::ifstream file(_filePath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        fprintf(stderr, "Failed to load file: %s\n", _filePath);
        return nullptr;
    }

    // Get the file size.
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read file contents into a buffer.
    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size))
    {
        fprintf(stderr, "Failed to read file: %s\n", _filePath);
        return nullptr;
    }

    // Create a BGFX memory block from the buffer.
    return bgfx::copy(buffer.data(), static_cast<uint32_t>(size));
}

bgfx::TextureHandle loadTextureDDS(const char* _filePath)
{
    const bgfx::Memory* mem = loadMem(_filePath);
    if (mem == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }
    // Create texture from memory.
    return bgfx::createTexture(mem);
}


static int instanceCounter = 0;

static void spawnInstance(Camera camera, const std::string& instanceName, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, std::vector<Instance*>& instances)
{

    float spawnDistance = 5.0f;
    // Position for new instance, e.g., random or predefined position
    float x = camera.position.x + camera.front.x * spawnDistance;
    float y = camera.position.y + camera.front.y * spawnDistance;
    float z = camera.position.z + camera.front.z * spawnDistance;

	std::string fullName = instanceName + std::to_string(instanceCounter);

    // Create a new instance with the current vertex and index buffers
    instances.push_back(new Instance(instanceCounter++, fullName, x, y, z, vbh, ibh));
    std::cout << "New instance created at (" << x << ", " << y << ", " << z << ")" << std::endl;
}
// Recursive draw function for hierarchy.
void drawInstance(const Instance* instance, bgfx::ProgramHandle program, bgfx::UniformHandle u_diffuseTex, bgfx::UniformHandle u_objectColor,
    bgfx::TextureHandle defaultWhiteTexture, bgfx::TextureHandle inheritedTexture, const float* parentTransform = nullptr)
{
    float local[16];
    bx::mtxSRT(local,
        instance->scale[0], instance->scale[1], instance->scale[2],
        instance->rotation[0], instance->rotation[1], instance->rotation[2],
        instance->position[0], instance->position[1], instance->position[2]);

    float world[16];
    if (parentTransform)
        bx::mtxMul(world, parentTransform, local);
    else
        std::memcpy(world, local, sizeof(world));


    const bgfx::VertexBufferHandle invalidVbh = BGFX_INVALID_HANDLE;
    const bgfx::IndexBufferHandle invalidIbh = BGFX_INVALID_HANDLE;
    // Draw geometry if valid.
    if (instance->vertexBuffer.idx != invalidVbh.idx &&
        instance->indexBuffer.idx != invalidIbh.idx)
    {
        bgfx::setTransform(world);
        // If this instance is selected, update the override uniform:
        if (selectedInstance == instance)
        {
            bgfx::setUniform(u_objectColor, instance->objectColor);
        }
        else
        {
            // Otherwise, use white (no override)
            float white[4] = { 1,1,1,1 };
            bgfx::setUniform(u_objectColor, white);
        }
        bgfx::setVertexBuffer(0, instance->vertexBuffer);
        bgfx::setIndexBuffer(instance->indexBuffer);
        // Decide which texture to use:
        // If the inherited texture (from the parent) is valid, then use it regardless of what the instance may have set.
        // Otherwise, use the instance’s own texture (if any), or fall back to the default.
        bgfx::TextureHandle textureToUse = defaultWhiteTexture;
        if (inheritedTexture.idx != bgfx::kInvalidHandle)
        {
            textureToUse = inheritedTexture;
        }
        else if (instance->diffuseTexture.idx != bgfx::kInvalidHandle)
        {
            textureToUse = instance->diffuseTexture;
        }
        else
        {
            textureToUse = defaultWhiteTexture;
        }
        bgfx::setTexture(1, u_diffuseTex, textureToUse);
        bgfx::submit(0, program);
    }
    // For children, propagate the override:
    // If the inherited texture is already valid, continue propagating that.
    // Otherwise, use the current instance’s texture as the inherited texture.
    bgfx::TextureHandle newInheritedTexture = inheritedTexture;
    if (inheritedTexture.idx == bgfx::kInvalidHandle)
    {
        newInheritedTexture = instance->diffuseTexture;
    }
    // Recursively draw children.
    for (const Instance* child : instance->children)
    {
        drawInstance(child, program, u_diffuseTex, u_objectColor, defaultWhiteTexture, newInheritedTexture, world);
    }
}
// Recursive deletion for hierarchy.
void deleteInstance(Instance* instance)
{
    for (Instance* child : instance->children)
    {
        deleteInstance(child);
    }
    delete instance;
}
// Recursive function to show the instance hierarchy in a tree view.
void ShowInstanceTree(Instance* instance, Instance*& selectedInstance)
{
    // Set up flags for the tree node.
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
    if (instance->children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selectedInstance == instance)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Use the instance pointer as the unique ID.
    bool nodeOpen = ImGui::TreeNodeEx((void*)instance, flags, "%s", instance->name.c_str());
    if (ImGui::IsItemClicked())
    {
        selectedInstance = instance;
    }

    // If the node is open (and it's not a leaf that doesn't push), display its children.
    if (nodeOpen && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (Instance* child : instance->children)
        {
            ShowInstanceTree(child, selectedInstance);
        }
        ImGui::TreePop();
    }
}

void saveInstance(std::ofstream& file, const Instance* instance, std::vector<TextureOption> availableTextures, int parentID = -1)
{
    file << (parentID == -1 ? "instance " : "child ") << instance->id << " " << instance->name << " "
        << instance->position[0] << " " << instance->position[1] << " " << instance->position[2] << " "
        << instance->rotation[0] << " " << instance->rotation[1] << " " << instance->rotation[2] << " "
        << instance->scale[0] << " " << instance->scale[1] << " " << instance->scale[2] << " "
        << instance->objectColor[0] << " " << instance->objectColor[1] << " " << instance->objectColor[2] << " " << instance->objectColor[3] << " ";

    // Find texture index
    int textureIndex = -1;
    for (int i = 0; i < availableTextures.size(); i++) {
        if (instance->diffuseTexture.idx == availableTextures[i].handle.idx) {
            textureIndex = i;
            break;
        }
    }
    file << textureIndex << "\n";

    // Save children
    for (const Instance* child : instance->children) {
        saveInstance(file, child, availableTextures, instance->id);
    }
}

// Recursive function to save an instance to a text file.
void saveScene(const std::string& filename, const std::vector<Instance*>& instances, std::vector<TextureOption> availableTextures)
{
	std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to save scene!" << std::endl;
        return;
    }

    for (const Instance* instance : instances) {
        saveInstance(file, instance, availableTextures);
    }

    file.close();
	std::cout << "Scene saved to " << filename << std::endl;
}

void loadSceneText(const std::string& filename, std::vector<Instance*>& instances, std::vector<TextureOption> availableTextures) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to load scene!" << std::endl;
        return;
    }

    std::cout << "Test" << std::endl;

    // Clear old instances safely
    for (Instance* inst : instances) {
        delete inst;
    }

    instances.clear();
    std::unordered_map<int, Instance*> instanceMap;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "instance" || type == "child") {
            int id, parentID = -1;
            std::string name;
            float pos[3], rot[3], scale[3], color[4];
            int textureIndex;

            if (type == "child") {
                iss >> parentID;
            }

            iss >> id >> name >> pos[0] >> pos[1] >> pos[2]
                >> rot[0] >> rot[1] >> rot[2]
                >> scale[0] >> scale[1] >> scale[2]
                >> color[0] >> color[1] >> color[2] >> color[3] >> textureIndex;

            Instance* instance = new Instance(id, name, pos[0], pos[1], pos[2], BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
            instance->rotation[0] = rot[0]; instance->rotation[1] = rot[1]; instance->rotation[2] = rot[2];
            instance->scale[0] = scale[0]; instance->scale[1] = scale[1]; instance->scale[2] = scale[2];
            instance->objectColor[0] = color[0]; instance->objectColor[1] = color[1];
            instance->objectColor[2] = color[2]; instance->objectColor[3] = color[3];

            // Assign texture if available
            if (textureIndex >= 0 && textureIndex < availableTextures.size()) {
                instance->diffuseTexture = availableTextures[textureIndex].handle;
            }

            instanceMap[id] = instance;

            if (parentID == -1) {
                instances.push_back(instance);
            }
            else {
                instanceMap[parentID]->addChild(instance);
            }
        }
    }

    file.close();
    std::cout << "Scene loaded from " << filename << std::endl;
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
	ImGuiWindowFlags window_flags = 0;
    //window_flags |= ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_Implbgfx_Init(255);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    // Load shaders

    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float) // NEW: UV coordinates
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

    //cornell box generation
    bgfx::VertexBufferHandle vbh_cornell = bgfx::createVertexBuffer(
        bgfx::makeRef(cornellBoxVertices, sizeof(cornellBoxVertices)),
        layout
    );

    bgfx::IndexBufferHandle ibh_cornell = bgfx::createIndexBuffer(
        bgfx::makeRef(cornellBoxIndices, sizeof(cornellBoxIndices))
    );

    bgfx::VertexBufferHandle vbh_innerCube = bgfx::createVertexBuffer(
        bgfx::makeRef(innerCubeVertices, sizeof(innerCubeVertices)),
        layout
    );
    bgfx::IndexBufferHandle ibh_innerCube = bgfx::createIndexBuffer(
        bgfx::makeRef(innerCubeIndices, sizeof(innerCubeIndices))
    );
    bgfx::VertexBufferHandle vbh_floor = bgfx::createVertexBuffer(
        bgfx::makeRef(cornellBoxFloorVertices, sizeof(cornellBoxFloorVertices)),
        layout
    );
    bgfx::IndexBufferHandle ibh_floor = bgfx::createIndexBuffer(
        bgfx::makeRef(cornellBoxFloorIndices, sizeof(cornellBoxFloorIndices))
    );
    bgfx::VertexBufferHandle vbh_ceiling = bgfx::createVertexBuffer(
        bgfx::makeRef(cornellBoxCeilingVertices, sizeof(cornellBoxCeilingVertices)),
        layout
    );
    bgfx::IndexBufferHandle ibh_ceiling = bgfx::createIndexBuffer(
        bgfx::makeRef(cornellBoxCeilingIndices, sizeof(cornellBoxCeilingIndices))
    );
    bgfx::VertexBufferHandle vbh_back = bgfx::createVertexBuffer(
        bgfx::makeRef(cornellBoxBackVertices, sizeof(cornellBoxBackVertices)),
        layout
    );
    bgfx::IndexBufferHandle ibh_back = bgfx::createIndexBuffer(
        bgfx::makeRef(cornellBoxBackIndices, sizeof(cornellBoxBackIndices))
    );
    bgfx::VertexBufferHandle vbh_left = bgfx::createVertexBuffer(
        bgfx::makeRef(cornellBoxLeftVertices, sizeof(cornellBoxLeftVertices)),
        layout
    );
    bgfx::IndexBufferHandle ibh_left = bgfx::createIndexBuffer(
        bgfx::makeRef(cornellBoxLeftIndices, sizeof(cornellBoxLeftIndices))
    );
    bgfx::VertexBufferHandle vbh_right = bgfx::createVertexBuffer(
        bgfx::makeRef(cornellBoxRightVertices, sizeof(cornellBoxRightVertices)),
        layout
    );
    bgfx::IndexBufferHandle ibh_right = bgfx::createIndexBuffer(
        bgfx::makeRef(cornellBoxRightIndices, sizeof(cornellBoxRightIndices))
    );

    //mesh generation
    MeshData meshData = loadMesh("meshes/suzanne.obj");
    bgfx::VertexBufferHandle vbh_mesh;
    bgfx::IndexBufferHandle ibh_mesh;
    createMeshBuffers(meshData, vbh_mesh, ibh_mesh);

    //teapot generation
    MeshData teapotData = loadMesh("meshes/teapot.obj");
    bgfx::VertexBufferHandle vbh_teapot;
    bgfx::IndexBufferHandle ibh_teapot;
    createMeshBuffers(teapotData, vbh_teapot, ibh_teapot);

    //stanford bunny generation
    MeshData bunnyData = loadMesh("meshes/bunny.obj");
    bgfx::VertexBufferHandle vbh_bunny;
    bgfx::IndexBufferHandle ibh_bunny;
    createMeshBuffers(bunnyData, vbh_bunny, ibh_bunny);

    //lucy generation
    MeshData lucyData = loadMesh("meshes/lucy.obj");
    bgfx::VertexBufferHandle vbh_lucy;
    bgfx::IndexBufferHandle ibh_lucy;
    createMeshBuffers(lucyData, vbh_lucy, ibh_lucy);

    //Load OBJ file
    /*std::vector<ObjLoader::Vertex> suzanneVertices;
    std::vector<uint16_t> suzanneIndices;
    if (!ObjLoader::loadObj("meshes/helicopter.obj", suzanneVertices, suzanneIndices)) {
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

    std::vector<Instance*> instances;
    instances.reserve(100);

    bool modelMovement = false;

    float lightColor[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    float lightDir[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    float scale[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    int spawnPrimitive = 0;

	bool* p_open = NULL;
    
    float inkColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Typically black ink.

    // Global uniform for the diffuse texture (put this at file scope or as a static variable)
    static bgfx::UniformHandle u_diffuseTex = BGFX_INVALID_HANDLE;
    u_diffuseTex = bgfx::createUniform("u_diffuseTex", bgfx::UniformType::Sampler);

    // Create uniforms (do this once)
    u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
    u_viewPos = bgfx::createUniform("u_viewPos", bgfx::UniformType::Vec4);

    bgfx::UniformHandle u_inkColor = bgfx::createUniform("u_inkColor", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_e = bgfx::createUniform("u_e", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_noiseTex = bgfx::createUniform("u_noiseTex", bgfx::UniformType::Sampler);
    bgfx::UniformHandle u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_objectColor = bgfx::createUniform("u_objectColor", bgfx::UniformType::Vec4);

    // SHADER TEXTURE
    bgfx::TextureHandle noiseTexture = loadTextureDDS("shaders\\noise1.dds");


    // Texture
    std::vector<TextureOption> availableTextures;
    {
        TextureOption tex;
        tex.name = "Rock";
        tex.handle = loadTextureDDS("shaders\\texture1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        tex.name = "Wood";
        tex.handle = loadTextureDDS("shaders\\texture2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // add texture
    }

    //plane
    bgfx::TextureHandle planeTexture = loadTextureDDS("shaders\\texture2.dds");
    // Create a default white texture once (static variable)
    static bgfx::TextureHandle defaultWhiteTexture = BGFX_INVALID_HANDLE;
    if (defaultWhiteTexture.idx == bgfx::kInvalidHandle)
    {
        const uint32_t whitePixel = 0xffffffff;
        const bgfx::Memory* mem = bgfx::copy(&whitePixel, sizeof(whitePixel));
        defaultWhiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::BGRA8, 0, mem);
    }

    // Load shaders and create program once
    bgfx::ShaderHandle vsh = loadShader("shaders\\v_out17.bin");
    bgfx::ShaderHandle fsh = loadShader("shaders\\f_out18.bin");

    bgfx::ProgramHandle defaultProgram = bgfx::createProgram(vsh, fsh, true);
    
    // --- Spawn Cornell Box with Hierarchy ---
    Instance* cornellBox = new Instance(instanceCounter++, "cornell_box", 8.0f, 0.0f, -5.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
    // Create a walls node (dummy instance without geometry)
    Instance* wallsNode = new Instance(instanceCounter++, "walls", 0.0f, 0.0f, 0.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);

    Instance* floorPlane = new Instance(instanceCounter++, "floor", 0.0f, 0.0f, 0.0f, vbh_floor, ibh_floor);
    wallsNode->addChild(floorPlane);
    Instance* ceilingPlane = new Instance(instanceCounter++, "ceiling", 0.0f, 0.0f, 0.0f, vbh_ceiling, ibh_ceiling);
    wallsNode->addChild(ceilingPlane);
    Instance* backPlane = new Instance(instanceCounter++, "back", 0.0f, 0.0f, 0.0f, vbh_back, ibh_back);
    wallsNode->addChild(backPlane);
    Instance* leftPlane = new Instance(instanceCounter++, "left_wall", 0.0f, 0.0f, 0.0f, vbh_left, ibh_left);
    wallsNode->addChild(leftPlane);
    Instance* rightPlane = new Instance(instanceCounter++, "right_wall", 0.0f, 0.0f, 0.0f, vbh_right, ibh_right);
    wallsNode->addChild(rightPlane);

    Instance* innerCube = new Instance(instanceCounter++, "inner_cube", 0.3f, 0.0f, 0.0f, vbh_innerCube, ibh_innerCube);
    Instance* innerRectBox = new Instance(instanceCounter++, "inner_rectbox", 1.1f, 1.0f, -0.9f, vbh_innerCube, ibh_innerCube);
    innerRectBox->scale[0] = 0.8f;
    innerRectBox->scale[1] = 2.0f;
    innerRectBox->scale[2] = 0.8f;
    cornellBox->addChild(wallsNode);
    cornellBox->addChild(innerCube);
    cornellBox->addChild(innerRectBox);
    instances.push_back(cornellBox);

    spawnInstance(camera, "teapot", vbh_teapot, ibh_teapot, instances);
    instances.back()->position[0] = 3.0f;
    instances.back()->position[1] = -1.0f;
    instances.back()->position[2] = -5.0f;
    instances.back()->scale[0] *= 0.03f;
    instances.back()->scale[1] *= 0.03f;
    instances.back()->scale[2] *= 0.03f;

    spawnInstance(camera, "bunny", vbh_bunny, ibh_bunny, instances);
    instances.back()->position[0] = -1.0f;
    instances.back()->position[1] = -1.0f;
    instances.back()->position[2] = -5.0f;
    instances.back()->scale[0] *= 10.0f;
    instances.back()->scale[1] *= 10.0f;
    instances.back()->scale[2] *= 10.0f;

    spawnInstance(camera, "lucy", vbh_lucy, ibh_lucy, instances);
    instances.back()->position[0] = -4.0f;
    instances.back()->position[1] = -1.0f;
    instances.back()->position[2] = -5.0f;
    instances.back()->scale[0] *= 0.01f;
    instances.back()->scale[1] *= 0.01f;
    instances.back()->scale[2] *= 0.01f;

    Logger::GetInstance();

    //MAIN LOOP
	while (!glfwWindowShouldClose(window))
	{
        glfwPollEvents();

        //imgui loop
		ImGui_ImplGlfw_NewFrame();
		ImGui_Implbgfx_NewFrame();
		ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
   //     ImGui::SetNextWindowPos(viewport->Pos);
   //     ImGui::SetNextWindowSize(viewport->Size);
   //     ImGui::SetNextWindowViewport(viewport->ID);

   //     ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_NoDocking;
   //     dockspace_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
   //     dockspace_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

   //     ImGui::Begin("Dockspace Window", nullptr, dockspace_flags);
   //     ImGui::DockSpace(ImGui::GetID("MainDockspace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
   //     ImGui::End();

   //     static bool firstTime = true;
   //     if (firstTime)
   //     {
   //         firstTime = false;
			//ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
   //         ImGui::DockBuilderRemoveNode(dockspace_id);
			//ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
			//ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

			//ImGuiID dock_main_id = dockspace_id;

   //         // First, split the main dock space to create a right-side docking area
   //         ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.3f, nullptr, &dock_main_id);

   //         // Now, split the right-side docking area **vertically**
   //         ImGuiID dock_top = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Left, 0.5f, nullptr, &dock_right);

   //         // Dock windows to the right side, stacked vertically
   //         ImGui::DockBuilderDockWindow("CrossHatchEditor", dock_top);  // Top half
   //         ImGui::DockBuilderDockWindow("Object List", dock_right);     // Bottom half

			//ImGui::DockBuilderFinish(dockspace_id);
   //     }
		
        //IMGUI WINDOW FOR CONTROLS
        //FOR REFERENCE USE THIS: https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
        ImGui::Begin("CrossHatchEditor", p_open, window_flags);
        if (ImGui::BeginMenuBar())
        { 
            if (ImGui::BeginMenu("File", true))
            {
                if (ImGui::MenuItem("Open..")) 
                {
                    std::string loadFilePath = openFileDialog(false); // Open load dialog
                    if (!loadFilePath.empty())
                        loadSceneText(loadFilePath, instances, availableTextures);
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) 
                { 
                    std::string saveFilePath = openFileDialog(true); // Open save dialog
                    if (!saveFilePath.empty())
                        saveScene(saveFilePath, instances, availableTextures);
                }
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
            ImGui::RadioButton("Cornell Box", &spawnPrimitive, 6);
            ImGui::RadioButton("Teapot", &spawnPrimitive, 7);
            ImGui::RadioButton("Bunny", &spawnPrimitive, 8);
            ImGui::RadioButton("Lucy", &spawnPrimitive, 9);
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
                else if (spawnPrimitive == 6)
                {
                    float spawnDistance = 5.0f;
                    // Position for new instance, e.g., random or predefined position
                    float x = camera.position.x + camera.front.x * spawnDistance;
                    float y = camera.position.y + camera.front.y * spawnDistance;
                    float z = camera.position.z + camera.front.z * spawnDistance;
                    // --- Spawn Cornell Box with Hierarchy ---
                    Instance* cornellBox = new Instance(instanceCounter++, "cornell_box", x, y, z, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
                    // Create a walls node (dummy instance without geometry)
                    Instance* wallsNode = new Instance(instanceCounter++, "walls", 0.0f, 0.0f, 0.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);

                    Instance* floorPlane = new Instance(instanceCounter++, "floor", 0.0f, 0.0f, 0.0f, vbh_floor, ibh_floor);
                    wallsNode->addChild(floorPlane);
                    Instance* ceilingPlane = new Instance(instanceCounter++, "ceiling", 0.0f, 0.0f, 0.0f, vbh_ceiling, ibh_ceiling);
                    wallsNode->addChild(ceilingPlane);
                    Instance* backPlane = new Instance(instanceCounter++, "back", 0.0f, 0.0f, 0.0f, vbh_back, ibh_back);
                    wallsNode->addChild(backPlane);
                    Instance* leftPlane = new Instance(instanceCounter++, "left_wall", 0.0f, 0.0f, 0.0f, vbh_left, ibh_left);
                    wallsNode->addChild(leftPlane);
                    Instance* rightPlane = new Instance(instanceCounter++, "right_wall", 0.0f, 0.0f, 0.0f, vbh_right, ibh_right);
                    wallsNode->addChild(rightPlane);

                    Instance* innerCube = new Instance(instanceCounter++, "inner_cube", 0.0f, 0.0f, 0.0f, vbh_innerCube, ibh_innerCube);
                    Instance* innerRectBox = new Instance(instanceCounter++, "inner_rectbox", 1.1f, 1.0f, -0.9f, vbh_innerCube, ibh_innerCube);
                    innerRectBox->scale[0] = 0.8f;
                    innerRectBox->scale[1] = 2.0f;
                    innerRectBox->scale[2] = 0.8f;
                    cornellBox->addChild(wallsNode);
                    cornellBox->addChild(innerCube);
                    cornellBox->addChild(innerRectBox);
                    instances.push_back(cornellBox);
                    std::cout << "Cornell Box spawned" << std::endl;
                }
                else if (spawnPrimitive == 7)
                {
                    spawnInstance(camera, "teapot", vbh_teapot, ibh_teapot, instances);
                    std::cout << "Teapot spawned" << std::endl;
                }
                else if (spawnPrimitive == 8)
                {
                    spawnInstance(camera, "bunny", vbh_bunny, ibh_bunny, instances);
                    std::cout << "Bunny spawned" << std::endl;
                }
                else if (spawnPrimitive == 9)
                {
                    spawnInstance(camera, "lucy", vbh_lucy, ibh_lucy, instances);
                    std::cout << "Lucy spawned" << std::endl;
                }
			}
			if (ImGui::Button("Remove Last Instance"))
			{
                Instance* inst = instances.back();
				instances.pop_back();
                //instanceCounter--;
                delete inst;
                selectedInstance = nullptr;
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
		ImGui::End();

        Logger::GetInstance().DrawImGuiLogger();

		ImGui::Begin("Object List", p_open, window_flags);
        static int selectedInstanceIndex = -1;
    
    
		if (ImGui::CollapsingHeader("Object List"))
		{
            // For each top-level instance, show its tree.
            for (Instance* instance : instances)
            {
                ShowInstanceTree(instance, selectedInstance);

            }
        
            // If an instance is selected, show its transform controls.
            if (selectedInstance)
            {
                ImGui::Separator();
                ImGui::Text("Selected: %s", selectedInstance->name.c_str());
                ImGui::DragFloat3("Translation", selectedInstance->position, 0.1f);
                ImGui::DragFloat3("Rotation (radians)", selectedInstance->rotation, 0.1f);
                ImGui::DragFloat3("Scale", selectedInstance->scale, 0.1f);
                //Object color Selection
                ImGui::Separator();
                ImGui::ColorEdit3("Object Color", selectedInstance->objectColor);
                // --- Diffuse Texture Selection ---
                ImGui::Separator();
                ImGui::Text("Texture:");

                // Use a static variable to hold the currently selected index
                // (If no texture is selected, set to -1)
                static int selectedTextureIndex = -1;

                // Create a combo box listing all available textures plus "None"
                if (ImGui::BeginCombo("##DiffuseTexture",
                    (selectedTextureIndex >= 0) ? availableTextures[selectedTextureIndex].name.c_str() : "None"))
                {
                    // Option "None": no diffuse texture is applied.
                    if (ImGui::Selectable("None", selectedTextureIndex == -1))
                    {
                        selectedTextureIndex = -1;
                        selectedInstance->diffuseTexture = BGFX_INVALID_HANDLE;
                    }
                    // List each available texture.
                    for (int i = 0; i < availableTextures.size(); i++)
                    {
                        bool is_selected = (selectedTextureIndex == i);
                        if (ImGui::Selectable(availableTextures[i].name.c_str(), is_selected))
                        {
                            selectedTextureIndex = i;
                            // Update the selected instance’s diffuse texture handle.
                            selectedInstance->diffuseTexture = availableTextures[i].handle;
                            std::cout << "Instance " << selectedInstance->name << " diffuseTexture handle: "
                                << selectedInstance->diffuseTexture.idx << std::endl;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // You can add a button to remove the selected instance from the hierarchy.
                if (ImGui::Button("Remove Selected"))
                {
                    // If the selected instance has a parent, remove it from the parent's children list.
                    if (selectedInstance->parent)
                    {
                        Instance* parent = selectedInstance->parent;
                        auto it = std::find(parent->children.begin(), parent->children.end(), selectedInstance);
                        if (it != parent->children.end())
                        {
                            parent->children.erase(it);
                        }
                    }
                    else
                    {
                        // Otherwise, it's top-level. Remove it from the global instances vector.
                        auto it = std::find(instances.begin(), instances.end(), selectedInstance);
                        if (it != instances.end())
                        {
                            instances.erase(it);
                        }
                    }

                    // Delete the instance (which will recursively delete its children)
                    deleteInstance(selectedInstance);
                    selectedInstance = nullptr;
                }
             }
        }
        if (ImGui::CollapsingHeader("Crosshatch Ink"))
        {
            // ColorEdit4 allows you to edit a vec4 (RGBA)
            ImGui::ColorEdit4("Ink Color", inkColor);
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
            Instance* inst = instances.back();
            instances.pop_back();
            //instanceCounter--;
            delete inst;
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
            lightDir[0] = 0.0f;
            lightDir[1] = 1.0f;
            lightDir[2] = 1.0f;
            lightDir[3] = 0.0f;
        }

        int width = static_cast<int>(viewport->Size.x);
        int height = static_cast<int>(viewport->Size.y);

		if (width == 0 || height == 0)
		{
			continue;
		}

        bgfx::reset(width, height, BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));

        float view[16];
        bx::mtxLookAt(view, camera.position, bx::add(camera.position, camera.front), camera.up);

        float proj[16];
        bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(0, view, proj);

        // Set model matrix
        float mtx[16];
        bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bgfx::setTransform(mtx);

        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        bgfx::touch(0);


        float viewPos[4] = { camera.position.x, camera.position.y, camera.position.z, 1.0f };

        bgfx::setUniform(u_lightDir, lightDir);
        bgfx::setUniform(u_lightColor, lightColor);
        bgfx::setUniform(u_viewPos, viewPos);

        float cameraPos[4] = { camera.position.x, camera.position.y, camera.position.z, 1.0f };
        float epsilon[4] = { 0.02f, 0.0f, 0.0f, 0.0f }; // Pack the epsilon value into the first element; the other three can be 0.
        
        bgfx::setUniform(u_inkColor, inkColor);
        bgfx::setUniform(u_cameraPos, cameraPos);
        bgfx::setUniform(u_e, epsilon);
        bgfx::setTexture(0, u_noiseTex, noiseTexture); // Bind the noise texture to texture stage 0.

        //bgfx::UniformHandle u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
        
        // Example parameter values
        float params[4] = { 0.02f, 15.0f, 0.0f, 0.0f }; // e = 0.02 (tolerance), scale = 15.0
        bgfx::setUniform(u_params, params);



        //bgfx::ShaderHandle vsh = loadShader("shaders\\v_out14.bin");
        //bgfx::ShaderHandle fsh = loadShader("shaders\\f_out14.bin");
        //bgfx::ProgramHandle defaultProgram = bgfx::createProgram(vsh, fsh, true);


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
        bgfx::setTexture(1, u_diffuseTex, planeTexture); // Bind the fixed plane texture:
        bgfx::submit(0, defaultProgram);

        if (modelMovement)
        {
            for (const Instance* instance : instances)
            {
                float model[16];
                // Calculate the forward vector pointing towards the camera
                bx::Vec3 modelToCamera = bx::normalize(bx::Vec3(camera.position.x - instance->position[0],
                    camera.position.y - instance->position[1],
                    camera.position.z - instance->position[2]));


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
                model[12] = instance->position[0];
                model[13] = instance->position[1];
                model[14] = instance->position[2];
                model[15] = 1.0f;
                bgfx::setTransform(model);
                

                // Draw each top-level instance recursively:
                drawInstance(instance, defaultProgram, u_diffuseTex, u_objectColor,defaultWhiteTexture, BGFX_INVALID_HANDLE);
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
                    instance->scale[0], instance->scale[1], instance->scale[2],
                    instance->rotation[0], instance->rotation[1], instance->rotation[2],
                    instance->position[0], instance->position[1], instance->position[2]);

                bgfx::setTransform(model);


                // Draw each top-level instance recursively:
                drawInstance(instance, defaultProgram, u_diffuseTex, u_objectColor,defaultWhiteTexture, BGFX_INVALID_HANDLE);
            }
        }

        // Update your vertex layout to include normals
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float) // NEW: UV coordinates
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
        bgfx::destroy(instance->vertexBuffer);
        bgfx::destroy(instance->indexBuffer);
        deleteInstance(instance);
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
    bgfx::destroy(vbh_cornell);
    bgfx::destroy(ibh_cornell);
    bgfx::destroy(vbh_innerCube);
    bgfx::destroy(ibh_innerCube);
	//bgfx::destroy(defaultProgram);
    ImGui_ImplGlfw_Shutdown();
	
    bgfx::shutdown();
    glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
