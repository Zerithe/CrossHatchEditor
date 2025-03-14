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
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif
#include <filesystem>
namespace fs = std::filesystem;

#include <bgfx/bgfx.h>
#include <bx/uint32_t.h>
#include <bgfx/platform.h>
#include <bx/commandline.h>
#include <bx/endian.h>
#include <bx/math.h>
#include <bx/readerwriter.h>
#include <bx/string.h>

#include <algorithm>
#include <string>

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

#include "Light.h"
//#include "DebugDraw.h"
#include <ImGuizmo.h>
#include <cmath> // for rad2deg, deg2rad, etc.
#include <cfloat> // For FLT_MAX and FLT_MIN

//png, jpg image support #1179
//reference: https://github.com/bkaradzic/bgfx/issues/1179
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static bool highlightVisible = true;
static float RadToDeg(float rad) { return rad * (180.0f / 3.14159265358979f); }
static float DegToRad(float deg) { return deg * (3.14159265358979f / 180.0f); }
static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

#define WNDW_WIDTH 1600
#define WNDW_HEIGHT 900

static bool s_showStats = false;
bgfx::UniformHandle u_lightDir;
bgfx::UniformHandle u_lightColor;
bgfx::UniformHandle u_viewPos;
//bgfx::UniformHandle u_scale;
// Define the picking render target dimensions.
#define PICKING_DIM 128

static bool useGlobalCrosshatchSettings = true;
// (Define TAU in C++ too)
const float TAU = 6.28318530718f;
// Declare static variables to hold our crosshatch parameters:
static float inkColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Typically black ink.
static float epsilonValue = 0.02f;              // Outer Line Smoothness or Epsilon
static float strokeMultiplier = 1.0f;           // Outer Hatch Density or Stroke Multiplier
static float lineAngle1 = TAU / 8.0f;           // Outer Hatch Angle or Line Angle 1
static float lineAngle2 = TAU / 16.0f;          // Line Angle 2

// These static variables will hold the extra parameter values.
static float patternScale = 3.0f;               // Outer Hatch Scale or Pattern Scale
static float lineThickness = 0.3f;              // Outer Hatch Weight or Line Thickness

// You can leave the remaining components as 0 (or later repurpose them)
static float transparencyValue = 1.0f;          // Hatch Opacity or Transparency
static int crosshatchMode = 2;                  // 0 = hatch ver 1.0, 1 = hatch ver 1.1, 2 = hatch ver 1.2, 3 = basic shader

// These static variables will hold the values for u_paramsLayer
static float layerPatternScale = 1.0f;          // Inner Hatch Scale or Layer Pattern Scale
static float layerStrokeMult = 0.250f;           // Inner Hatch Density or Layer Stroke Multiplier
static float layerAngle = 2.983f;               // Inner Hatch Angle or Layer Angle
static float layerLineThickness = 10.0f;        // Inner Hatch Weight or Layer Line Thickness

bgfx::TextureHandle noiseTexture = BGFX_INVALID_HANDLE;

static bgfx::UniformHandle u_uvTransform = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle u_albedoFactor = BGFX_INVALID_HANDLE;

// Global uniform for the diffuse texture (put this at file scope or as a static variable)
static bgfx::UniformHandle u_diffuseTex = BGFX_INVALID_HANDLE;

static bgfx::TextureHandle s_pickingRT = BGFX_INVALID_HANDLE;
static bgfx::TextureHandle s_pickingRTDepth = BGFX_INVALID_HANDLE;
static bgfx::FrameBufferHandle s_pickingFB = BGFX_INVALID_HANDLE;
static bgfx::TextureHandle s_pickingReadTex = BGFX_INVALID_HANDLE;
static uint8_t s_pickingBlitData[PICKING_DIM * PICKING_DIM * 4] = { 0 };

// Uniform for the picking shader that outputs the object ID as color.
static bgfx::UniformHandle u_id = BGFX_INVALID_HANDLE;
// Program handle for the picking pass.
static bgfx::ProgramHandle pickingProgram = BGFX_INVALID_HANDLE;

struct TextureOption {
    std::string name;
    bgfx::TextureHandle handle;
};

std::vector<TextureOption> availableNoiseTextures;
int currentNoiseIndex = 0; // which noise texture is selected by the user
int globalCurrentNoiseIndex = 0; // which noise texture is selected by the user

struct MaterialParams
{
    float tiling[2];     // (tilingU, tilingV)
    float offset[2];     // (offsetU, offsetV)
    float albedo[4];     // (r, g, b, a)
};

// Define a struct to hold animation parameters for a light.
struct LightAnimation {
    bool enabled = false;                       // Toggle animation on/off.
    float amplitude[3] = { 6.0f, 0.0f, 0.0f };  // Amplitude for x, y, z motion.
    float frequency[3] = { 1.0f, 1.0f, 1.0f };  // Frequency (Hz) for each axis.
    float phase[3]     = { 0.0f, 0.0f, 0.0f };  // Phase offset for each axis.
};

struct Instance
{
    int id;
    std::string name;
    std::string type;
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

    // NEW: noise texture
    bgfx::TextureHandle noiseTexture = BGFX_INVALID_HANDLE;

    MaterialParams material;

    // --- New for lights ---
    bool isLight = false;
    LightProperties lightProps; // Valid if isLight == true.

    // NEW: Store the base (original) position for animation.
    float basePosition[3];

    // NEW: Animation parameters for lights.
    LightAnimation lightAnim;

    // NEW: For light objects only – determines if the debug visual (the sphere)
    // is drawn. (Default true.)
    bool showDebugVisual = true;

    std::vector<Instance*> children; // Hierarchy: child instances
    Instance* parent = nullptr;      // pointer to parent

    float inkColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // Declare variables to hold our crosshatch parameters:
    float epsilonValue = 0.02f;             // Outer Line Smoothness or Epsilon
    float strokeMultiplier = 1.0f;          // Outer Hatch Density or Stroke Multiplier
    float lineAngle1 = TAU / 8.0f;          // Outer Hatch Angle or Line Angle 1
    float lineAngle2 = TAU / 16.0f;         // Line Angle 2

    // These variables will hold the extra parameter values.
    float patternScale = 3.0f;              // Outer Hatch Scale or Pattern Scale
    float lineThickness = 0.3f;             // Outer Hatch Weight or Line Thickness

    // You can leave the remaining components as 0 (or later repurpose them)
    float transparencyValue = 1.0f;         // Hatch Opacity or Transparency
    int crosshatchMode = 2;                 // 0 = hatch ver 1.0, 1 = hatch ver 1.1, 2 = hatch ver 1.2, 3 = basic shader

    // These variables will hold the values for u_paramsLayer
    float layerPatternScale = 1.0f;         // Inner Hatch Scale or Layer Pattern Scale
    float layerStrokeMult = 0.250f;         // Inner Hatch Density or Layer Stroke Multiplier
    float layerAngle = 2.983f;              // Inner Hatch Angle or Layer Angle
    float layerLineThickness = 10.0f;       // Inner Hatch Weight or Layer Line Thickness

    //variables for rotating spot light
    float centerX = 0.0f;
    float centerZ = 0.0f;
    float radius = 5.0f;
    float rotationSpeed = 0.5f;
    float instanceAngle = 0.0f;

    Instance(int instanceId, const std::string& instanceName, const std::string& instanceType, float x, float y, float z, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh)
        : id(instanceId), name(instanceName), type(instanceType), vertexBuffer(vbh), indexBuffer(ibh)
    {
        position[0] = x;
        position[1] = y;
        position[2] = z;
        // Initialize with no rotation and uniform scale of 1
        rotation[0] = rotation[1] = rotation[2] = 0.0f;
        scale[0] = scale[1] = scale[2] = 1.0f;
        // Initialize the object color to white (no override)
        objectColor[0] = 1.0f; objectColor[1] = 1.0f; objectColor[2] = 1.0f; objectColor[3] = 1.0f;
        // For light objects, default to drawing the debug visual.
        showDebugVisual = true;

        material.tiling[0] = 1.0f;
        material.tiling[1] = 1.0f;
        material.offset[0] = 0.0f;
        material.offset[1] = 0.0f;
        material.albedo[0] = 1.0f; // r
        material.albedo[1] = 1.0f; // g
        material.albedo[2] = 1.0f; // b
        material.albedo[3] = 1.0f; // a

        noiseTexture = availableNoiseTextures[0].handle;
        // Store base position for animation.
        basePosition[0] = x; basePosition[1] = y; basePosition[2] = z;
    }
    void addChild(Instance* child) {
        children.push_back(child);
        child->parent = this;
    }
};
static Instance* selectedInstance = nullptr;

struct MeshData {
    std::vector<PosColorVertex> vertices;
    std::vector<uint32_t> indices;
};

struct Vec3 {
    float x, y, z;
};

void BuildWorldMatrix(const Instance* inst, float* outMatrix) {
    float local[16];
    float translation[3] = { inst->position[0], inst->position[1], inst->position[2] };
    float rotationDeg[3] = { RadToDeg(inst->rotation[0]), RadToDeg(inst->rotation[1]), RadToDeg(inst->rotation[2]) };
    float scale[3] = { inst->scale[0], inst->scale[1], inst->scale[2] };
    // Build the local transform matrix.
    ImGuizmo::RecomposeMatrixFromComponents(translation, rotationDeg, scale, local);

    if (inst->parent)
    {
        float parentWorld[16];
        // Recursively compute the parent's world matrix.
        BuildWorldMatrix(inst->parent, parentWorld);
        // Multiply parent's world matrix with the local transform.
        bx::mtxMul(outMatrix, local, parentWorld);
    }
    else
    {
        memcpy(outMatrix, local, sizeof(float) * 16);
    }
}
void BuildMatrixFromInstance_ImGuizmo(const Instance* inst, float* outMatrix)
{
    // 1) Copy your instance’s data into the arrays ImGuizmo expects:
    float translation[3] = { inst->position[0], inst->position[1], inst->position[2] };
    float rotationDeg[3] = {
        RadToDeg(inst->rotation[0]),
        RadToDeg(inst->rotation[1]),
        RadToDeg(inst->rotation[2])
    };
    float scl[3] = { inst->scale[0], inst->scale[1], inst->scale[2] };

    // 2) Build the matrix:
    ImGuizmo::RecomposeMatrixFromComponents(translation, rotationDeg, scl, outMatrix);
}

void DecomposeMatrixToInstance_ImGuizmo(const float* matrix, Instance* inst)
{
    float translation[3];
    float rotationDeg[3];
    float scl[3];

    ImGuizmo::DecomposeMatrixToComponents(matrix, translation, rotationDeg, scl);

    // Copy them back:
    inst->position[0] = translation[0];
    inst->position[1] = translation[1];
    inst->position[2] = translation[2];

    // Convert degrees to radians if your system is in radians:
    inst->rotation[0] = DegToRad(rotationDeg[0]);
    inst->rotation[1] = DegToRad(rotationDeg[1]);
    inst->rotation[2] = DegToRad(rotationDeg[2]);

    inst->scale[0] = scl[0];
    inst->scale[1] = scl[1];
    inst->scale[2] = scl[2];
}

void DrawGizmoForSelected(Instance* selectedInstance, const float* view, const float* proj)
{
    if (!selectedInstance)
        return;

    // 1) Build world matrix from the selected instance (correctly accounting for parent hierarchy)
    float matrix[16];
    BuildWorldMatrix(selectedInstance, matrix);

    // 2) Setup ImGuizmo
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // 3) Store the original object's local transform before manipulation
    float localMatrix[16];
    bx::mtxSRT(localMatrix,
        selectedInstance->scale[0], selectedInstance->scale[1], selectedInstance->scale[2],
        selectedInstance->rotation[0], selectedInstance->rotation[1], selectedInstance->rotation[2],
        selectedInstance->position[0], selectedInstance->position[1], selectedInstance->position[2]);

    // Check if CTRL is held down to enable snapping
    bool useSnap = io.KeyCtrl;

    // Define snap settings for different operations (x, y, z for each operation)
    float snapTranslation[3] = { 0.5f, 0.5f, 0.5f };  // Snap all axes to 0.5 units
    float snapRotation[3] = { 90.0f, 90.0f, 90.0f };  // Snap all axes to 90 degrees
    float snapScale[3] = { 0.5f, 0.5f, 0.5f };        // Snap all axes to 0.5 scale incr

    // 4) Manipulate the world matrix
    bool changed = ImGuizmo::Manipulate(
        view, proj,
        currentGizmoOperation,
        currentGizmoMode,
        matrix,
        nullptr,
        useSnap ? (currentGizmoOperation == ImGuizmo::TRANSLATE ? snapTranslation :
            currentGizmoOperation == ImGuizmo::ROTATE ? snapRotation :
            snapScale) : nullptr
    );

    // 5) If changed, convert world matrix back to local space
    if (changed)
    {
        if (selectedInstance->parent)
        {
            // Get parent's world matrix
            float parentWorld[16];
            BuildWorldMatrix(selectedInstance->parent, parentWorld);

            // Calculate parent's inverse matrix
            float parentInverse[16];
            bx::mtxInverse(parentInverse, parentWorld);

            // Convert world-space result back to local space
            float newLocalMatrix[16];
            bx::mtxMul(newLocalMatrix, matrix, parentInverse);

            // Extract the local transform values
            float translation[3];
            float rotationDeg[3];
            float scale[3];
            ImGuizmo::DecomposeMatrixToComponents(newLocalMatrix, translation, rotationDeg, scale);

            // Apply the local transform values
            selectedInstance->position[0] = translation[0];
            selectedInstance->position[1] = translation[1];
            selectedInstance->position[2] = translation[2];

            // Convert degrees to radians and apply snapping if CTRL is held
            for (int i = 0; i < 3; i++) {
                float rotRad = DegToRad(rotationDeg[i]);

                // Apply additional rotation snapping if CTRL is held
                if (useSnap && currentGizmoOperation == ImGuizmo::ROTATE) {
                    // Convert to degrees, snap, and back to radians
                    float degrees = RadToDeg(rotRad);
                    float snapped = round(degrees / 90.0f) * 90.0f;
                    rotRad = DegToRad(snapped);
                }

                selectedInstance->rotation[i] = rotRad;
            }

            selectedInstance->scale[0] = scale[0];
            selectedInstance->scale[1] = scale[1];
            selectedInstance->scale[2] = scale[2];
        }
        else
        {
            // For root objects (no parent), decompose directly
            float translation[3];
            float rotationDeg[3];
            float scale[3];
            ImGuizmo::DecomposeMatrixToComponents(matrix, translation, rotationDeg, scale);

            // Apply values
            selectedInstance->position[0] = translation[0];
            selectedInstance->position[1] = translation[1];
            selectedInstance->position[2] = translation[2];

            // Apply rotations with snapping if needed
            for (int i = 0; i < 3; i++) {
                float rotRad = DegToRad(rotationDeg[i]);

                // Apply rotation snapping if CTRL is held
                if (useSnap && currentGizmoOperation == ImGuizmo::ROTATE) {
                    float degrees = RadToDeg(rotRad);
                    float snapped = round(degrees / 90.0f) * 90.0f;
                    rotRad = DegToRad(snapped);
                }

                selectedInstance->rotation[i] = rotRad;
            }

            selectedInstance->scale[0] = scale[0];
            selectedInstance->scale[1] = scale[1];
            selectedInstance->scale[2] = scale[2];
        }
    }
}
//transfer to ObjLoader.cpp
void computeNormals(std::vector<PosColorVertex>& vertices, const std::vector<uint32_t>& indices) {

    // Reset all normals to zero
    for (auto& vertex : vertices) {
        vertex.nx = 0.0f;
        vertex.ny = 0.0f;
        vertex.nz = 0.0f;
    }
    // Create an array to accumulate normals
    std::vector<Vec3> accumulatedNormals(vertices.size(), { 0.0f, 0.0f, 0.0f });

    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        Vec3 v0 = { vertices[i0].x, vertices[i0].y, vertices[i0].z };
        Vec3 v1 = { vertices[i1].x, vertices[i1].y, vertices[i1].z };
        Vec3 v2 = { vertices[i2].x, vertices[i2].y, vertices[i2].z };

        Vec3 edge1 = { v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
        Vec3 edge2 = { v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };

        Vec3 normal = {
            edge2.y * edge1.z - edge2.z * edge1.y,
            edge2.z * edge1.x - edge2.x * edge1.z,
            edge2.x * edge1.y - edge2.y * edge1.x
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
    std::vector<uint32_t> indices;

    // used for making origin of imported objects at 0 0 0 to center gizmo
    // works for both single mesh and multi mesh objects
    // Global bounding box for the entire model.
    float globalMinX = FLT_MAX, globalMinY = FLT_MAX, globalMinZ = FLT_MAX;
    float globalMaxX = -FLT_MAX, globalMaxY = -FLT_MAX, globalMaxZ = -FLT_MAX;

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

            // Update global bounding box.
            if (vertex.x < globalMinX) globalMinX = vertex.x;
            if (vertex.y < globalMinY) globalMinY = vertex.y;
            if (vertex.z < globalMinZ) globalMinZ = vertex.z;
            if (vertex.x > globalMaxX) globalMaxX = vertex.x;
            if (vertex.y > globalMaxY) globalMaxY = vertex.y;
            if (vertex.z > globalMaxZ) globalMaxZ = vertex.z;

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

            vertices.push_back(vertex);
        }


        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            // Reverse the winding order
            for (int j = face.mNumIndices - 1; j >= 0; j--) {
                indices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[j]));
            }
        }

        // Compute normals if the mesh does not have them
        if (!mesh->HasNormals()) {
            computeNormals(vertices, indices);
        }
    }

    // Compute the global center.
    float centerX = (globalMinX + globalMaxX) * 0.5f;
    float centerY = (globalMinY + globalMaxY) * 0.5f;
    float centerZ = (globalMinZ + globalMaxZ) * 0.5f;

    // Second pass: recenter all vertices by subtracting the global center.
    for (auto& v : vertices) {
        v.x -= centerX;
        v.y -= centerY;
        v.z -= centerZ;
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
        bgfx::copy(meshData.vertices.data(), sizeof(PosColorVertex) * meshData.vertices.size()),
        layout
    );

    // Detect if we need 32-bit indices
    if (meshData.vertices.size() > std::numeric_limits<uint16_t>::max()) {
        std::cout << "Using 32-bit index buffer due to high poly count.\n";
        ibh = bgfx::createIndexBuffer(
            bgfx::copy(meshData.indices.data(), sizeof(uint32_t) * meshData.indices.size()),
            BGFX_BUFFER_INDEX32 // Enables 32-bit index buffer
        );
    }
    else {
        std::cout << "Using 16-bit index buffer for efficiency.\n";
        std::vector<uint16_t> indices16(meshData.indices.begin(), meshData.indices.end());
        ibh = bgfx::createIndexBuffer(
            bgfx::copy(indices16.data(), sizeof(uint16_t) * indices16.size())
        );
    }
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

// The new function that checks extension and calls either your DDS loader or stb_image.
bgfx::TextureHandle loadTextureFile(const char* filePath)
{
    fs::path p(filePath);
    std::string ext = p.extension().string();
    // Convert extension to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // If DDS, use your existing function
    if (ext == ".dds")
    {
        return loadTextureDDS(filePath);
    }
    else
    {
        // Otherwise, use stb_image to load the file
        int width, height, channels;
        // Force RGBA (4 channels)
        unsigned char* data = stbi_load(filePath, &width, &height, &channels, 4);
        if (!data)
        {
            std::cerr << "Failed to load image via stb_image: " << filePath << std::endl;
            return BGFX_INVALID_HANDLE;
        }

        // Create a BGFX memory block from the raw pixels
        // We now have width * height * 4 bytes (RGBA).
        const bgfx::Memory* mem = bgfx::copy(data, width * height * 4);

        // Free stb_image’s data
        stbi_image_free(data);

        // Create a 2D texture from that memory
        // Note: If you want MIP maps, pass e.g. BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
        //       then call bgfx::generateMips(...) or generate them offline.
        bgfx::TextureHandle texHandle = bgfx::createTexture2D(
            (uint16_t)width,
            (uint16_t)height,
            false,          // Has MIPs?
            1,              // Number of layers
            bgfx::TextureFormat::RGBA8, // We loaded RGBA
            0,              // Flags
            mem
        );

        if (!bgfx::isValid(texHandle))
        {
            std::cerr << "Failed to create BGFX texture from: " << filePath << std::endl;
        }
        else
        {
            std::cout << "Successfully loaded image (stb_image): " << filePath
                << "  (" << width << "x" << height << ")\n";
        }
        return texHandle;
    }
}

static int instanceCounter = 0;

static void spawnInstance(Camera camera, const std::string& instanceName, const std::string& instanceType, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, std::vector<Instance*>& instances)
{

    float spawnDistance = 5.0f;
    // Position for new instance, e.g., random or predefined position
    float x = camera.position.x + camera.front.x * spawnDistance;
    float y = camera.position.y + camera.front.y * spawnDistance;
    float z = camera.position.z + camera.front.z * spawnDistance;

    std::string fullName = instanceName + std::to_string(instanceCounter);

    // Create a new instance with the current vertex and index buffers
    instances.push_back(new Instance(instanceCounter++, fullName, instanceType, x, y, z, vbh, ibh));
    std::cout << "New instance created at (" << x << ", " << y << ", " << z << ")" << std::endl;
}

static void spawnInstanceAtCenter(const std::string& instanceName, const std::string& instanceType, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh, std::vector<Instance*>& instances)
{
    std::string fullName = instanceName + std::to_string(instanceCounter);

    // Create a new instance with the current vertex and index buffers
    instances.push_back(new Instance(instanceCounter++, fullName, instanceType, 0.0f, 0.0f, 0.0f, vbh, ibh));
    std::cout << "New instance created at (" << 0 << ", " << 0 << ", " << 0 << ")" << std::endl;
}

static void spawnLight(const Camera& camera, bgfx::VertexBufferHandle vbh_sphere, bgfx::IndexBufferHandle ibh_sphere, bgfx::VertexBufferHandle vbh_cone, bgfx::IndexBufferHandle ibh_cone, std::vector<Instance*>& instances)
{
    float spawnDistance = 5.0f;
    float x = camera.position.x + camera.front.x * spawnDistance;
    float y = camera.position.y + camera.front.y * spawnDistance;
    float z = camera.position.z + camera.front.z * spawnDistance;
    std::string lightName = "light" + std::to_string(instanceCounter);
    Instance* lightInst = new Instance(instanceCounter++, lightName, "light", x, y, z, vbh_sphere, ibh_sphere);
    lightInst->isLight = true;
    // Set default light properties (point light)
    lightInst->lightProps.type = LightType::Point;
    lightInst->lightProps.intensity = 1.0f;
    lightInst->lightProps.range = 15.0f;
    lightInst->lightProps.coneAngle = 1.0f;
    lightInst->lightProps.color[0] = 1.0f; lightInst->lightProps.color[1] = 1.0f;
    lightInst->lightProps.color[2] = 1.0f; lightInst->lightProps.color[3] = 1.0f;

    // Update the debug geometry based on the light type.
    switch (lightInst->lightProps.type)
    {
    case LightType::Point:
        // Use sphere: already set
        break;
    case LightType::Spot:
        // Use cone mesh for a spot light.
        lightInst->vertexBuffer = vbh_cone;
        lightInst->indexBuffer = ibh_cone;
        break;
    case LightType::Directional:
        // use a cone for now
        lightInst->vertexBuffer = vbh_cone;
        lightInst->indexBuffer = ibh_cone;
        break;
    default:
        break;
    }

    // Make the visual representation small.
    lightInst->scale[0] = lightInst->scale[1] = lightInst->scale[2] = 0.2f;
    instances.push_back(lightInst);
    std::cout << "Spawned light " << lightName << " at (" << x << ", " << y << ", " << z << ")\n";
}

// Helper function to determine if a color is (approximately) white.
bool IsWhite(const float color[4], float epsilon = 0.001f)
{
    return std::fabs(color[0] - 1.0f) < epsilon &&
        std::fabs(color[1] - 1.0f) < epsilon &&
        std::fabs(color[2] - 1.0f) < epsilon &&
        std::fabs(color[3] - 1.0f) < epsilon;
}
// Recursive draw function for hierarchy.
void drawInstance(const Instance* instance, bgfx::ProgramHandle defaultProgram, bgfx::ProgramHandle lightDebugProgram, bgfx::UniformHandle u_noiseTex, bgfx::UniformHandle u_diffuseTex, bgfx::UniformHandle u_objectColor, bgfx::UniformHandle u_tint, bgfx::UniformHandle u_inkColor, bgfx::UniformHandle u_e, bgfx::UniformHandle u_params, bgfx::UniformHandle u_extraParams, bgfx::UniformHandle u_paramsLayer,
    bgfx::TextureHandle defaultWhiteTexture, bgfx::TextureHandle inheritedNoiseTex, bgfx::TextureHandle inheritedTexture, const float* parentColor = nullptr, const float* parentTransform = nullptr)
{
    float local[16];
    bx::mtxSRT(local,
        instance->scale[0], instance->scale[1], instance->scale[2],
        instance->rotation[0], instance->rotation[1], instance->rotation[2],
        instance->position[0], instance->position[1], instance->position[2]);

    float world[16];
    if (parentTransform)
        bx::mtxMul(world, local, parentTransform);
    else
        std::memcpy(world, local, sizeof(world));

    // Compute effective object color.
    float effectiveColor[4];
    if (parentColor && !IsWhite(parentColor))
    {
        // If parent's color is not white, inherit it.
        std::memcpy(effectiveColor, parentColor, sizeof(effectiveColor));
    }
    else
    {
        // Otherwise, use this instance's objectColor.
        std::memcpy(effectiveColor, instance->objectColor, sizeof(effectiveColor));
    }

    // Set the object override color uniform.
    bgfx::setUniform(u_objectColor, effectiveColor);

    const float tintBasic[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
    const float tintHighlighted[4] = { 0.3f, 0.3f, 2.0f, 0.1f };
    if (selectedInstance == instance && highlightVisible) {
        bgfx::setUniform(u_tint, tintHighlighted);
    }
    else {
        bgfx::setUniform(u_tint, tintBasic);
    }
    if (!useGlobalCrosshatchSettings) {
        bgfx::setUniform(u_inkColor, instance->inkColor);
        // Set epsilon uniform:
        float epsilonUniform[4] = { instance->epsilonValue, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(u_e, epsilonUniform);

        // Prepare an array of 4 floats.
        // Set u_params uniform:
        float paramsUniform[4] = { 0.0f, instance->strokeMultiplier, instance->lineAngle1, instance->lineAngle2 };
        bgfx::setUniform(u_params, paramsUniform);

        // Prepare an array of 4 floats.
        float extraParamsUniform[4] = { instance->patternScale, instance->lineThickness, instance->transparencyValue, float(instance->crosshatchMode) };
        // Set the uniform for extra parameters.
        bgfx::setUniform(u_extraParams, extraParamsUniform);


        // Prepare an array of 4 floats.
        float paramsLayerUniform[4] = { instance->layerPatternScale, instance->layerStrokeMult, instance->layerAngle, instance->layerLineThickness };
        // Set the uniform for extra parameters.
        bgfx::setUniform(u_paramsLayer, paramsLayerUniform);
    }
    const bgfx::VertexBufferHandle invalidVbh = BGFX_INVALID_HANDLE;
    const bgfx::IndexBufferHandle invalidIbh = BGFX_INVALID_HANDLE;
    // Draw geometry if valid.
    if (instance->vertexBuffer.idx != invalidVbh.idx &&
        instance->indexBuffer.idx != invalidIbh.idx)
    {
        bgfx::setTransform(world);
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

        // Decide which noise tex to use
        bgfx::TextureHandle noiseTextureToUse;
        if (useGlobalCrosshatchSettings) {
            noiseTextureToUse = noiseTexture;
        }
        else if (inheritedNoiseTex.idx != bgfx::kInvalidHandle) {
            noiseTextureToUse = inheritedNoiseTex;
        }
        else if (instance->noiseTexture.idx != bgfx::kInvalidHandle)
        {
            noiseTextureToUse = instance->noiseTexture;
        }
        else
        {
            noiseTextureToUse = availableNoiseTextures[0].handle;
        }
        bgfx::setTexture(0, u_noiseTex, noiseTextureToUse);

        // If the instance is a light and its debug visual is turned off,
        // skip drawing the sphere representation.
        if (instance->type == "light" && !instance->showDebugVisual)
        {
            // Do nothing (or you might draw an alternative marker).
        }
        else
        {
            // If this instance is a light, use the debug shader; otherwise, use default.
            bgfx::submit(0, (instance->type == "light") ? lightDebugProgram : defaultProgram);
        }
    }
    // Determine what color to pass to children.
    // If the effective color is white, then children should use their own objectColor.
    const float* childParentColor = (!IsWhite(effectiveColor)) ? effectiveColor : nullptr;
    // For children, propagate the override:
    // If the inherited texture is already valid, continue propagating that.
    // Otherwise, use the current instance’s texture as the inherited texture.
    bgfx::TextureHandle newInheritedTexture = inheritedTexture;
    if (inheritedTexture.idx == bgfx::kInvalidHandle)
    {
        newInheritedTexture = instance->diffuseTexture;
    }
    
    // For children
    bgfx::TextureHandle newInheritedNoiseTex = inheritedNoiseTex;
    if (inheritedTexture.idx == bgfx::kInvalidHandle)
    {
        newInheritedNoiseTex = instance->noiseTexture;
    }

    // Recursively draw children.
    for (const Instance* child : instance->children)
    {
        drawInstance(child, defaultProgram, lightDebugProgram, u_noiseTex, u_diffuseTex, u_objectColor, u_tint, u_inkColor, u_e, u_params, u_extraParams, u_paramsLayer, defaultWhiteTexture, inheritedNoiseTex, newInheritedTexture, childParentColor, world);
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
void ShowInstanceTree(Instance* instance, Instance*& selectedInstance, std::vector<Instance*>& instances)
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
        if (selectedInstance == instance)
            selectedInstance = nullptr;
        else
            selectedInstance = instance;
    }

    // Add right-click context menu for renaming
    if (ImGui::BeginPopupContextItem())
    {
        static char nameBuffer[256];
        if (ImGui::IsWindowAppearing())
        {
            // Copy the instance name to the buffer when the popup first appears
            strncpy(nameBuffer, instance->name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';  // Ensure null termination
        }

        ImGui::Text("Rename %s", instance->name.c_str());
        ImGui::Separator();

        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##rename", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            instance->name = std::string(nameBuffer);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Apply"))
        {
            instance->name = std::string(nameBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Begin drag source
    if (ImGui::BeginDragDropSource())
    {
        // Set the payload: the pointer to the instance
        Instance* dragInstance = instance;
        ImGui::SetDragDropPayload("DND_INSTANCE", &dragInstance, sizeof(Instance*));
        ImGui::Text("%s", instance->name.c_str());
        ImGui::EndDragDropSource();
    }
    // Make this node a drag drop target
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_INSTANCE"))
        {
            // Get the instance being dragged
            Instance* dropped = *(Instance**)payload->Data;
            if (dropped != instance)
            {
                // Remove the dropped instance from its current parent's children list
                if (dropped->parent)
                {
                    auto it = std::find(dropped->parent->children.begin(),
                        dropped->parent->children.end(), dropped);
                    if (it != dropped->parent->children.end())
                        dropped->parent->children.erase(it);
                }
                else
                {
                    // If it's top-level, remove it from the global instances vector.
                    auto it = std::find(instances.begin(), instances.end(), dropped);
                    if (it != instances.end())
                        instances.erase(it);
                }
                // Add the dropped instance as a child of the current node.
                instance->addChild(dropped);
            }
        }
        ImGui::EndDragDropTarget();
    }
    // If the node is open (and it's not a leaf that doesn't push), display its children.
    if (nodeOpen && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
    {
        for (Instance* child : instance->children)
        {
            ShowInstanceTree(child, selectedInstance, instances);
        }
        ImGui::TreePop();
    }
}


void saveInstance(std::ofstream& file, const Instance* instance,
    const std::vector<TextureOption>& availableTextures, int parentID = -1)
{
    std::string textureName = "none";
    for (const auto& tex : availableTextures)
    {
        if (instance->diffuseTexture.idx == tex.handle.idx)
        {
            textureName = tex.name;
            break;
        }
    }

    std::string noiseTextureName = "none";
    for (const auto& tex : availableNoiseTextures)
    {
        if (instance->noiseTexture.idx == tex.handle.idx)
        {
            noiseTextureName = tex.name;
            break;
        }
    }

    file << instance->id << " " << instance->type << " " << instance->name << " "
        << instance->position[0] << " " << instance->position[1] << " " << instance->position[2] << " "
        << instance->rotation[0] << " " << instance->rotation[1] << " " << instance->rotation[2] << " "
        << instance->scale[0] << " " << instance->scale[1] << " " << instance->scale[2] << " "
        << instance->objectColor[0] << " " << instance->objectColor[1] << " " << instance->objectColor[2] << " " << instance->objectColor[3] << " "
        << textureName << " " << noiseTextureName << " " << parentID << " " << static_cast<int>(instance->lightProps.type) << " " << instance->lightProps.direction[0] << " "
        << instance->lightProps.direction[1] << " " << instance->lightProps.direction[2] << " " << instance->lightProps.intensity << " "
        << instance->lightProps.range << " " << instance->lightProps.coneAngle << " " << instance->lightProps.color[0] << " "
        << instance->lightProps.color[1] << " " << instance->lightProps.color[2] << " " << instance->lightProps.color[3] << " "
        << instance->inkColor[0] << " " << instance->inkColor[1] << " " << instance->inkColor[2] << " " << instance->inkColor[3] << " "
        << instance->epsilonValue << " " << instance->strokeMultiplier << " " << instance->lineAngle1 << " " << instance->lineAngle2 << " "
        << instance->patternScale << " " << instance->lineThickness << " " << instance->transparencyValue << " " << instance->crosshatchMode << " "
        << instance->layerPatternScale << " " << instance->layerStrokeMult << " " << instance->layerAngle << " " << instance->layerLineThickness << " "
        << instance->centerX << " " << instance->centerZ << " " << instance->radius << " " << instance->rotationSpeed << " " << instance->instanceAngle << " "
        << instance->basePosition[0] << " " << instance->basePosition[1] << " " << instance->basePosition[2] << " " 
        << instance->lightAnim.amplitude[0] << " " << instance->lightAnim.amplitude[1] << " " << instance->lightAnim.amplitude[2] << " " 
        << instance->lightAnim.frequency[0] << " " << instance->lightAnim.frequency[1] << " " << instance->lightAnim.frequency[2] << " "
        << instance->lightAnim.phase[0] << " " << instance->lightAnim.phase[1] << " " << instance->lightAnim.phase[2] << " "
        << static_cast<int>(instance->lightAnim.enabled) << "\n"; // Save `type` and parentID

    // Recursively save children
    for (const Instance* child : instance->children)
    {
        saveInstance(file, child, availableTextures, instance->id);
    }
}
void SaveImportedObjMap(const std::unordered_map<std::string, std::string>& map, const std::string& filePath)
{
    std::ofstream ofs(filePath);
    if (!ofs)
    {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return;
    }
    // Write each mapping as: key [whitespace] value
    for (const auto& entry : map)
    {
        ofs << entry.first << " " << entry.second << "\n";
    }
    ofs.close();
}
std::unordered_map<std::string, std::string> LoadImportedObjMap(const std::string& filePath)
{
    std::unordered_map<std::string, std::string> map;
    std::ifstream ifs(filePath);
    if (!ifs)
    {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        return map;
    }
    std::string key, path;
    while (ifs >> key >> path)
    {
        map[key] = path;
    }
    ifs.close();
    return map;
}

void saveSceneToFile(std::vector<Instance*>& instances, const std::vector<TextureOption>& availableTextures, const std::unordered_map<std::string, std::string>& importedObjMap)
{
    std::string saveFilePath = openFileDialog(true); // Open save dialog
    if (saveFilePath.empty()) return; // Exit if no file was chosen

    std::ofstream file(saveFilePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to save scene!" << std::endl;
        return;
    }

    for (const Instance* instance : instances)
    {
        // Ensure top-level instances are saved first (with no parent)
        saveInstance(file, instance, availableTextures, -1);
    }

    file.close();
    std::cout << "Scene saved to " << saveFilePath << std::endl;

    std::string importedObjMapPath = fs::path(saveFilePath).stem().string() + "_imp_obj_map.txt";
    SaveImportedObjMap(importedObjMap, importedObjMapPath);
    std::cout << "Imported obj paths saved to " << importedObjMapPath << std::endl;
}

std::string ConvertBackslashesToForward(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

// ========================
// UPDATED IMPORTER CODE WITH TEXTURE LOADING
// ========================

// Structure to hold mesh data, its transform, and its diffuse texture.
struct ImportedMesh {
    MeshData meshData;
    aiMatrix4x4 transform; // Global transform (accumulated from the scene hierarchy)
    bgfx::TextureHandle diffuseTexture; // Diffuse texture for this mesh, if available.
};

// Recursive function to traverse the scene graph.
// The additional 'baseDir' parameter lets us resolve relative texture paths.
void processNode(const aiScene* scene, aiNode* node, const aiMatrix4x4& parentTransform,
    const std::string& baseDir, std::vector<ImportedMesh>& importedMeshes)
{
    aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

    // Process each mesh referenced by this node.
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        unsigned int meshIndex = node->mMeshes[i];
        aiMesh* mesh = scene->mMeshes[meshIndex];
        MeshData meshData;
        size_t baseIndex = meshData.vertices.size();

        // Process vertices.
        for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
            PosColorVertex vertex;
            vertex.x = mesh->mVertices[j].x;
            vertex.y = mesh->mVertices[j].y;
            vertex.z = mesh->mVertices[j].z;
            if (mesh->HasNormals()) {
                vertex.nx = mesh->mNormals[j].x;
                vertex.ny = mesh->mNormals[j].y;
                vertex.nz = mesh->mNormals[j].z;
            }
            else {
                vertex.nx = vertex.ny = vertex.nz = 0.0f;
            }
            if (mesh->HasTextureCoords(0)) {
                vertex.u = mesh->mTextureCoords[0][j].x;
                vertex.v = mesh->mTextureCoords[0][j].y;
            }
            else {
                vertex.u = vertex.v = 0.0f;
            }
            if (mesh->HasVertexColors(0)) {
                vertex.abgr = ((uint8_t)(mesh->mColors[0][j].r * 255) << 24) |
                    ((uint8_t)(mesh->mColors[0][j].g * 255) << 16) |
                    ((uint8_t)(mesh->mColors[0][j].b * 255) << 8) |
                    (uint8_t)(mesh->mColors[0][j].a * 255);
            }
            else {
                vertex.abgr = 0xffffffff;
            }
            meshData.vertices.push_back(vertex);
        }

        // Process indices (reversed winding order).
        for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
            aiFace face = mesh->mFaces[j];
            for (int k = face.mNumIndices - 1; k >= 0; k--) {
                meshData.indices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[k]));
            }
        }

        // Recompute normals if missing.
        if (!mesh->HasNormals()) {
            computeNormals(meshData.vertices, meshData.indices);
        }

        // Create an ImportedMesh to store this mesh’s data.
        ImportedMesh impMesh;
        impMesh.meshData = meshData;
        impMesh.transform = globalTransform;
        impMesh.diffuseTexture = BGFX_INVALID_HANDLE;

        // --- New: Retrieve diffuse texture from the material ---
        // Attempt to load a texture from the material.
        if (scene->HasMaterials()) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString texPath;
            // For glTF, check for the base color texture.
            if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
                std::string textureFile = texPath.C_Str();
                fs::path fullTexPath = fs::path(baseDir) / textureFile;
                std::string normalizedTexPath = ConvertBackslashesToForward(fullTexPath.string());
                std::cout << "[DEBUG] Found baseColor texture: " << normalizedTexPath << std::endl;
                bgfx::TextureHandle texHandle = loadTextureFile(normalizedTexPath.c_str());
                if (bgfx::isValid(texHandle)) {
                    std::cout << "[DEBUG] Successfully loaded texture: " << normalizedTexPath << std::endl;
                    impMesh.diffuseTexture = texHandle;
                }
                else {
                    std::cout << "[DEBUG] FAILED to load texture: " << normalizedTexPath << std::endl;
                }
            }
            // Fallback: if no baseColor texture, try diffuse.
            else if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                std::string textureFile = texPath.C_Str();
                fs::path fullTexPath = fs::path(baseDir) / textureFile;
                std::string normalizedTexPath = ConvertBackslashesToForward(fullTexPath.string());
                std::cout << "[DEBUG] Found diffuse texture (fallback): " << normalizedTexPath << std::endl;
                bgfx::TextureHandle texHandle = loadTextureFile(normalizedTexPath.c_str());
                if (bgfx::isValid(texHandle)) {
                    std::cout << "[DEBUG] Successfully loaded texture: " << normalizedTexPath << std::endl;
                    impMesh.diffuseTexture = texHandle;
                }
                else {
                    std::cout << "[DEBUG] FAILED to load texture: " << normalizedTexPath << std::endl;
                }
            }
            else {
                std::cout << "[DEBUG] No baseColor or diffuse texture found for material index "
                    << mesh->mMaterialIndex << std::endl;
            }
        }

        importedMeshes.push_back(impMesh);
    }

    // Recursively process child nodes.
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(scene, node->mChildren[i], globalTransform, baseDir, importedMeshes);
    }
}

// Load the file and extract all meshes, their transforms, and diffuse textures.
// The baseDir is computed from the model file path.
std::vector<ImportedMesh> loadImportedMeshes(const std::string& filePath) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_PreTransformVertices);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Error: Assimp - " << importer.GetErrorString() << std::endl;
        return {};
    }

    std::vector<ImportedMesh> importedMeshes;
    aiMatrix4x4 identity; // Identity matrix
    // Determine the base directory from the model file path.
    fs::path modelPath(filePath);
    std::string baseDir = modelPath.parent_path().string();
    processNode(scene, scene->mRootNode, identity, baseDir, importedMeshes);

    // ---- Per–Mesh Recentering (as before) ----
    for (auto& impMesh : importedMeshes) {
        aiVector3D localMin(FLT_MAX, FLT_MAX, FLT_MAX);
        aiVector3D localMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (const auto& vertex : impMesh.meshData.vertices) {
            aiVector3D pos(vertex.x, vertex.y, vertex.z);
            localMin.x = std::min(localMin.x, pos.x);
            localMin.y = std::min(localMin.y, pos.y);
            localMin.z = std::min(localMin.z, pos.z);
            localMax.x = std::max(localMax.x, pos.x);
            localMax.y = std::max(localMax.y, pos.y);
            localMax.z = std::max(localMax.z, pos.z);
        }
        aiVector3D meshCenter = (localMin + localMax) * 0.5f;
        // Shift vertices so the mesh geometry is centered at (0,0,0)
        for (auto& vertex : impMesh.meshData.vertices) {
            vertex.x -= meshCenter.x;
            vertex.y -= meshCenter.y;
            vertex.z -= meshCenter.z;
        }
        // Update the mesh's transform to account for the shift.
        aiMatrix4x4 translationMat;
        aiMatrix4x4::Translation(meshCenter, translationMat);
        impMesh.transform = impMesh.transform * translationMat;
    }

    return importedMeshes;
}

std::unordered_map<std::string, std::string> loadSceneFromFile(std::vector<Instance*>& instances,
    const std::vector<TextureOption>& availableTextures,
    const std::unordered_map<std::string, std::pair<bgfx::VertexBufferHandle, bgfx::IndexBufferHandle>>& bufferMap)
{
    selectedInstance = nullptr;
    std::string loadFilePath = openFileDialog(false);
    std::string importedObjMapPath = fs::path(loadFilePath).stem().string() + "_imp_obj_map.txt";
    std::unordered_map<std::string, std::string> importedObjMap = LoadImportedObjMap(importedObjMapPath);
    if (loadFilePath.empty()) return importedObjMap;

    std::ifstream file(loadFilePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to load scene!" << std::endl;
        return importedObjMap;
    }

    // Clear existing instances
    for (Instance* inst : instances)
    {
        deleteInstance(inst);
    }
    instances.clear();

    std::unordered_map<int, Instance*> instanceMap; // Stores instances by their IDs
    std::vector<std::pair<int, int>> parentAssignments; // Stores parent-child assignments

    std::vector<ImportedMesh> importedMeshes;
    std::string importedMeshesName = "";

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        bgfx::TextureHandle diffuseTexture = BGFX_INVALID_HANDLE;
        int id, parentID, lightType, crosshatchMode, animationEnabled;
        std::string type, name, textureName, noiseTextureName;
        float pos[3], rot[3], scale[3], color[4], lightDirection[3], intensity, range, coneAngle, lightColor[4], inkColor[4], epsilonValue, strokeMultiplier, lineAngle1, lineAngle2, patternScale, lineThickness, transparencyValue, layerPatternScale, layerStrokeMult, layerAngle, layerLineThickness, centerX, centerZ, radius, rotationSpeed, instanceAngle, basePosition[3], amplitude[3], frequency[3], phase[3];

        iss >> id >> type >> name
            >> pos[0] >> pos[1] >> pos[2]
            >> rot[0] >> rot[1] >> rot[2]
            >> scale[0] >> scale[1] >> scale[2]
            >> color[0] >> color[1] >> color[2] >> color[3]
            >> textureName >> noiseTextureName >> parentID >> lightType
            >> lightDirection[0] >> lightDirection[1] >> lightDirection[2]
            >> intensity >> range >> coneAngle
            >> lightColor[0] >> lightColor[1] >> lightColor[2] >> lightColor[3]
            >> inkColor[0] >> inkColor[1] >> inkColor[2] >> inkColor[3]
            >> epsilonValue >> strokeMultiplier >> lineAngle1 >> lineAngle2
            >> patternScale >> lineThickness >> transparencyValue >> crosshatchMode
            >> layerPatternScale >> layerStrokeMult >> layerAngle >> layerLineThickness
            >> centerX >> centerZ >> radius >> rotationSpeed >> instanceAngle
            >> basePosition[0] >> basePosition[1] >> basePosition[2]
            >> amplitude[0] >> amplitude[1] >> amplitude[2]
			>> frequency[0] >> frequency[1] >> frequency[2]
            >> phase[0] >> phase[1] >> phase[2]
            >> animationEnabled;

        // Fetch correct buffers using `type`
        bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;

        auto it = bufferMap.find(type);
        if (it != bufferMap.end())
        {
            vbh = it->second.first;
            ibh = it->second.second;
        }
        else {
            size_t pos = type.find('_');
            std::string meshType = type.substr(0, pos);
            int meshNumber = std::stoi(type.substr(pos + 1));
            auto i = importedObjMap.find(meshType);
            if (i != importedObjMap.end())
            {
				if (importedMeshesName != meshType)
				{
					importedMeshes.clear();
					importedMeshes = loadImportedMeshes(i->second);
					importedMeshesName = meshType;
				}
                createMeshBuffers(importedMeshes[meshNumber].meshData, vbh, ibh);
                diffuseTexture = importedMeshes[meshNumber].diffuseTexture;
            }
        }

        // Create instance
        Instance* instance = new Instance(id, name, type, pos[0], pos[1], pos[2], vbh, ibh);
        instance->rotation[0] = rot[0]; instance->rotation[1] = rot[1]; instance->rotation[2] = rot[2];
        instance->scale[0] = scale[0]; instance->scale[1] = scale[1]; instance->scale[2] = scale[2];
        instance->objectColor[0] = color[0]; instance->objectColor[1] = color[1];
        instance->objectColor[2] = color[2]; instance->objectColor[3] = color[3];
        instance->lightProps.type = static_cast<LightType>(lightType);
        instance->lightProps.direction[0] = lightDirection[0]; instance->lightProps.direction[1] = lightDirection[1]; instance->lightProps.direction[2] = lightDirection[2];
        instance->lightProps.intensity = intensity;
        instance->lightProps.range = range;
        instance->lightProps.coneAngle = coneAngle;
        instance->lightProps.color[0] = lightColor[0]; instance->lightProps.color[1] = lightColor[1]; instance->lightProps.color[2] = lightColor[2]; instance->lightProps.color[3] = lightColor[3];
        if (instance->type == "light") {
            instance->isLight = true;
            if (instance->lightProps.type == LightType::Spot || instance->lightProps.type == LightType::Directional) {
                auto it = bufferMap.find("cone");
                if (it != bufferMap.end())
                {
                    instance->vertexBuffer = it->second.first;
                    instance->indexBuffer = it->second.second;
                }
            }
        }
        instance->inkColor[0] = inkColor[0]; instance->inkColor[1] = inkColor[1]; instance->inkColor[2] = inkColor[2]; instance->inkColor[3] = inkColor[3];
        instance->epsilonValue = epsilonValue;
        instance->strokeMultiplier = strokeMultiplier;
        instance->lineAngle1 = lineAngle1;
        instance->lineAngle2 = lineAngle2;
        instance->patternScale = patternScale;
        instance->lineThickness = lineThickness;
        instance->transparencyValue = transparencyValue;
        instance->crosshatchMode = crosshatchMode;
        instance->layerPatternScale = layerPatternScale;
        instance->layerStrokeMult = layerStrokeMult;
        instance->layerAngle = layerAngle;
        instance->layerLineThickness = layerLineThickness;
        instance->centerX = centerX;
        instance->centerZ = centerZ;
        instance->radius = radius;
        instance->rotationSpeed = rotationSpeed;
        instance->instanceAngle = instanceAngle;
		instance->basePosition[0] = basePosition[0]; instance->basePosition[1] = basePosition[1]; instance->basePosition[2] = basePosition[2];
		instance->lightAnim.amplitude[0] = amplitude[0]; instance->lightAnim.amplitude[1] = amplitude[1]; instance->lightAnim.amplitude[2] = amplitude[2];
		instance->lightAnim.frequency[0] = frequency[0]; instance->lightAnim.frequency[1] = frequency[1]; instance->lightAnim.frequency[2] = frequency[2];
		instance->lightAnim.phase[0] = phase[0]; instance->lightAnim.phase[1] = phase[1]; instance->lightAnim.phase[2] = phase[2];
		instance->lightAnim.enabled = static_cast<bool>(animationEnabled);

        // Assign texture
        if (textureName != "none")
        {
            for (const auto& tex : availableTextures)
            {
                if (tex.name == textureName)
                {
                    instance->diffuseTexture = tex.handle;
                    break;
                }
            }
        }
        else {
			instance->diffuseTexture = diffuseTexture;
        }

		// Assign noise texture
		if (noiseTextureName != "none")
		{
			for (const auto& tex : availableNoiseTextures)
			{
				if (tex.name == noiseTextureName)
				{
					instance->noiseTexture = tex.handle;
					break;
				}
			}
		}

        instanceMap[id] = instance;

        // If it has a parent, store the relationship
        if (parentID != -1)
        {
            parentAssignments.push_back({ id, parentID });
        }
        else
        {
            // If it has no parent, it's a top-level instance
            instances.push_back(instance);
        }
    }

    // Restore parent-child relationships
    for (const auto& [childID, parentID] : parentAssignments)
    {
        auto childIt = instanceMap.find(childID);
        auto parentIt = instanceMap.find(parentID);
        if (childIt != instanceMap.end() && parentIt != instanceMap.end())
        {
            parentIt->second->addChild(childIt->second);
        }
    }

    file.close();
    std::cout << "Scene loaded from " << loadFilePath << std::endl;
    for (const auto* inst : instances)
    {
        instanceCounter = std::max(instanceCounter, inst->id);
    }
    instanceCounter++;
    return importedObjMap;
}


void ShowTopLevelDropTarget(std::vector<Instance*>& instances)
{
    // Reserve a region across the available width (adjust the height as needed)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(avail.x, 50)); // 50 pixels tall drop area
    ImGui::Text("Drop here to make top-level");

    // Get the region's rectangle
    ImVec2 dropPos = ImGui::GetItemRectMin();
    ImVec2 dropSize = ImGui::GetItemRectSize();
    ImRect dropRect(dropPos, ImVec2(dropPos.x + dropSize.x, dropPos.y + dropSize.y));
    // Use a custom drag-drop target on that dummy widget.
    if (ImGui::BeginDragDropTargetCustom(dropRect, ImGui::GetID("TopLevelDropTarget")))
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_INSTANCE"))
        {
            Instance* dropped = *(Instance**)payload->Data;
            // Remove the dropped node from its current parent (if any)
            if (dropped->parent)
            {
                auto it = std::find(dropped->parent->children.begin(), dropped->parent->children.end(), dropped);
                if (it != dropped->parent->children.end())
                    dropped->parent->children.erase(it);
                dropped->parent = nullptr;
            }
            // If the node isn't already top-level, add it to the global list.
            auto it = std::find(instances.begin(), instances.end(), dropped);
            if (it == instances.end())
            {
                instances.push_back(dropped);
            }
        }
        ImGui::EndDragDropTarget();
    }
}
std::string OpenFileDialog(HWND owner, const char* filter)
{
#ifdef _WIN32
    OPENFILENAME ofn;
    char fileName[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner; // Use the passed HWND
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "";

    if (GetOpenFileName(&ofn))
    {
        return std::string(fileName);
    }
#endif
    return std::string();
}

std::string GetRelativePath(const std::string& absolutePath, const std::string& base = fs::current_path().string())
{
    fs::path absPath(absolutePath);
    fs::path basePath(base);
    fs::path relPath = fs::relative(absPath, basePath);
    return relPath.string();
}

Instance* findInstanceById(const std::vector<Instance*>& instances, int id)
{
    for (Instance* inst : instances)
    {
        if (inst->id == id)
            return inst;
        // Recursively check children.
        Instance* childResult = findInstanceById(inst->children, id);
        if (childResult != nullptr)
            return childResult;
    }
    return nullptr;
}

void renderInstancePickingRecursive(const Instance* instance, const float* parentTransform, uint32_t viewID)
{
    // Compute the local transform.
    float local[16];
    bx::mtxSRT(local,
        instance->scale[0], instance->scale[1], instance->scale[2],
        instance->rotation[0], instance->rotation[1], instance->rotation[2],
        instance->position[0], instance->position[1], instance->position[2]
    );

    // Compute the world transform by combining the parent's transform (if any) with the local.
    float world[16];
    if (parentTransform)
    {
        bx::mtxMul(world, local, parentTransform);
    }
    else
    {
        std::memcpy(world, local, sizeof(world));
    }

    bgfx::setTransform(world);

    // Encode the instance's unique ID into a color.
    float idColor[4] = { instance->id / 255.0f, 0.0f, 0.0f, 1.0f };
    bgfx::setUniform(u_id, idColor);

    // Submit the geometry if valid.
    const bgfx::VertexBufferHandle invalidVbh = BGFX_INVALID_HANDLE;
    const bgfx::IndexBufferHandle invalidIbh = BGFX_INVALID_HANDLE;
    // Draw geometry if valid.
    if (instance->vertexBuffer.idx != invalidVbh.idx &&
        instance->indexBuffer.idx != invalidIbh.idx)
    {
        bgfx::setVertexBuffer(0, instance->vertexBuffer);
        bgfx::setIndexBuffer(instance->indexBuffer);
        bgfx::submit(viewID, pickingProgram);
    }

    // Recursively render children with the new world transform.
    for (const Instance* child : instance->children)
    {
        renderInstancePickingRecursive(child, world, viewID);
    }
}

//-----------------------------------------------------------------------------
// Uniforms for light data (for shader)
const int MAX_LIGHTS = 16;
static bgfx::UniformHandle u_lights;   // array of vec4's (MAX_LIGHTS*4)
static bgfx::UniformHandle u_numLights;  // vec4 (x holds number of lights)

void collectLights(const Instance* inst, float* lightsData, int& numLights, const float* parentTransform = nullptr)
{
    if (!inst)
        return;

    // Use local matrix for this instance
    float local[16];
    bx::mtxSRT(local,
        inst->scale[0], inst->scale[1], inst->scale[2],
        inst->rotation[0], inst->rotation[1], inst->rotation[2],
        inst->position[0], inst->position[1], inst->position[2]);

    // Compute world matrix by combining with parent transform (if any)
    float world[16];
    if (parentTransform) {
        bx::mtxMul(world, local, parentTransform);
    }
    else {
        std::memcpy(world, local, sizeof(world));
    }

    if (inst->isLight)
    {
        if (numLights >= MAX_LIGHTS)
            return;

        int base = numLights * 16;

        // Type and intensity remain unchanged
        lightsData[base + 0] = static_cast<float>(inst->lightProps.type); // type
        lightsData[base + 1] = inst->lightProps.intensity;
        lightsData[base + 2] = 0.0f;
        lightsData[base + 3] = 0.0f;

        // Transform position from local to world space
        float worldPos[3] = { 0.0f, 0.0f, 0.0f };
        worldPos[0] = world[12]; // Translation is stored in elements 12, 13, 14
        worldPos[1] = world[13];
        worldPos[2] = world[14];

        lightsData[base + 4] = worldPos[0];
        lightsData[base + 5] = worldPos[1];
        lightsData[base + 6] = worldPos[2];
        lightsData[base + 7] = 1.0f;

        // For direction, we need to transform by the rotation part of the matrix only
        // (without translation), and then normalize the result
        float direction[3] = {
            inst->lightProps.direction[0],
            inst->lightProps.direction[1],
            inst->lightProps.direction[2]
        };

        // Transform direction vector using the 3x3 rotation part of the world matrix
        float worldDir[3] = { 0.0f, 0.0f, 0.0f };
        worldDir[0] = world[0] * direction[0] + world[4] * direction[1] + world[8] * direction[2];
        worldDir[1] = world[1] * direction[0] + world[5] * direction[1] + world[9] * direction[2];
        worldDir[2] = world[2] * direction[0] + world[6] * direction[1] + world[10] * direction[2];

        // Normalize the direction
        float length = std::sqrt(worldDir[0] * worldDir[0] + worldDir[1] * worldDir[1] + worldDir[2] * worldDir[2]);
        if (length > 0.0001f) {
            worldDir[0] /= length;
            worldDir[1] /= length;
            worldDir[2] /= length;
        }

        lightsData[base + 8] = worldDir[0];
        lightsData[base + 9] = worldDir[1];
        lightsData[base + 10] = worldDir[2];
        lightsData[base + 11] = inst->lightProps.coneAngle;

        // Light color remains unchanged
        lightsData[base + 12] = inst->lightProps.color[0];
        lightsData[base + 13] = inst->lightProps.color[1];
        lightsData[base + 14] = inst->lightProps.color[2];
        lightsData[base + 15] = inst->lightProps.range;

        numLights++;
    }

    // Process children with the current world transform
    for (const Instance* child : inst->children)
    {
        if (numLights >= MAX_LIGHTS)
            break;
        collectLights(child, lightsData, numLights, world);
    }
}

// Add this to your code - preferably right before main() or in a utility file
void updateRotatingLights(std::vector<Instance*>& instances, float deltaTime) {
    for (Instance* inst : instances) {
        if (inst->isLight && inst->name.find("rotating_light") != std::string::npos) {
            // Update angle by speed factor 
            inst->instanceAngle -= deltaTime * inst->rotationSpeed;

            if (inst->instanceAngle < 0) {
                inst->instanceAngle += TAU;  // Keep the angle within [0, TAU]
            }
            else if (inst->instanceAngle > TAU) {
                inst->instanceAngle -= TAU;  // Keep the angle within [0, TAU]
            }
            // Update position
            inst->position[0] = inst->centerX + inst->radius * cos(inst->instanceAngle);
            inst->position[2] = inst->centerZ + inst->radius * sin(inst->instanceAngle);

            // Calculate direction vector to center point (0,0,0)
            float dirX = inst->centerX - inst->position[0];
            float dirZ = inst->centerZ - inst->position[2];

            // Normalize the direction
            float length = sqrt(dirX * dirX + dirZ * dirZ);
            if (length > 0.001f) {
                dirX /= length;
                dirZ /= length;

                // Convert direction to rotation angles
                // Yaw (Y-axis rotation) - determines the horizontal orientation
                inst->rotation[1] = atan2(dirZ, dirX) + 3.14159f / 2.0f;

                // Keep roll (Z-axis rotation) at 0
                inst->rotation[2] = 0.0f;
            }
        }
    }
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

    /*ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));*/


    // Load shaders
    // Create an off-screen picking render target (color)
    s_pickingRT = bgfx::createTexture2D(PICKING_DIM, PICKING_DIM, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    // Create a depth texture for the picking RT.
    s_pickingRTDepth = bgfx::createTexture2D(PICKING_DIM, PICKING_DIM, false, 1,
        bgfx::TextureFormat::D32F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    bgfx::TextureHandle rt[2] =
    {
        s_pickingRT,
        s_pickingRTDepth
    };
    // Create a framebuffer using both the color and depth textures.
    s_pickingFB = bgfx::createFrameBuffer(BX_COUNTOF(rt), rt, true);

    // Create a CPU-readback texture for blitting.
    s_pickingReadTex = bgfx::createTexture2D(PICKING_DIM, PICKING_DIM, false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );

    // Create the uniform for picking – this will be set per-object.
    u_id = bgfx::createUniform("u_id", bgfx::UniformType::Vec4);

    // Load the picking shader program.
    // (Assumes you have compiled picking shaders "vs_picking_shaded.bin" and "fs_picking_id.bin")
    bgfx::ShaderHandle vsPick = loadShader("shaders\\vs_picking_shaded.bin");
    bgfx::ShaderHandle fsPick = loadShader("shaders\\fs_picking_id.bin");
    pickingProgram = bgfx::createProgram(vsPick, fsPick, true);

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

    // Cone generation
    std::vector<PosColorVertex> coneVertices;
    std::vector<uint16_t> coneIndices;

    generateCone(1.0f, 2.0f, 20, coneVertices, coneIndices);

    bgfx::VertexBufferHandle vbh_cone = bgfx::createVertexBuffer(
        bgfx::makeRef(coneVertices.data(), sizeof(PosColorVertex) * coneVertices.size()),
        layout
    );

    bgfx::IndexBufferHandle ibh_cone = bgfx::createIndexBuffer(
        bgfx::makeRef(coneIndices.data(), sizeof(uint16_t) * coneIndices.size())
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

    std::unordered_map<std::string, std::pair<bgfx::VertexBufferHandle, bgfx::IndexBufferHandle>> bufferMap;

    bufferMap["cube"] = { vbh_cube, ibh_cube };
    bufferMap["capsule"] = { vbh_capsule, ibh_capsule };
    bufferMap["cylinder"] = { vbh_cylinder, ibh_cylinder };
    bufferMap["sphere"] = { vbh_sphere, ibh_sphere };
    bufferMap["plane"] = { vbh_plane, ibh_plane };
    bufferMap["cornell_box"] = { vbh_cornell, ibh_cornell };
    bufferMap["innerCube"] = { vbh_innerCube, ibh_innerCube };
    bufferMap["floor"] = { vbh_floor, ibh_floor };
    bufferMap["ceiling"] = { vbh_ceiling, ibh_ceiling };
    bufferMap["back"] = { vbh_back, ibh_back };
    bufferMap["left"] = { vbh_left, ibh_left };
    bufferMap["right"] = { vbh_right, ibh_right };
    bufferMap["mesh"] = { vbh_mesh, ibh_mesh };
    bufferMap["teapot"] = { vbh_teapot, ibh_teapot };
    bufferMap["bunny"] = { vbh_bunny, ibh_bunny };
    bufferMap["lucy"] = { vbh_lucy, ibh_lucy };
    bufferMap["light"] = { vbh_sphere, ibh_sphere };
	bufferMap["cone"] = { vbh_cone, ibh_cone };
    bufferMap["empty"] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };


    std::unordered_map<std::string, std::string> importedObjMap;

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
    float scale[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

    int spawnPrimitive = 0;

    bool* p_open = NULL;

    u_diffuseTex = bgfx::createUniform("u_diffuseTex", bgfx::UniformType::Sampler);

    // Create uniforms (do this once)
    u_lights = bgfx::createUniform("u_lights", bgfx::UniformType::Vec4, MAX_LIGHTS * 4);
    u_numLights = bgfx::createUniform("u_numLights", bgfx::UniformType::Vec4);

    u_viewPos = bgfx::createUniform("u_viewPos", bgfx::UniformType::Vec4);

    bgfx::UniformHandle u_inkColor = bgfx::createUniform("u_inkColor", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_e = bgfx::createUniform("u_e", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_noiseTex = bgfx::createUniform("u_noiseTex", bgfx::UniformType::Sampler);
    bgfx::UniformHandle u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_objectColor = bgfx::createUniform("u_objectColor", bgfx::UniformType::Vec4);
    bgfx::UniformHandle u_tint = bgfx::createUniform("u_tint", bgfx::UniformType::Vec4);

    // Create a uniform for extra parameters as a vec4.
    bgfx::UniformHandle u_extraParams = bgfx::createUniform("u_extraParams", bgfx::UniformType::Vec4);

    // Create a uniform for extra parameters as a vec4.
    bgfx::UniformHandle u_paramsLayer = bgfx::createUniform("u_paramsLayer", bgfx::UniformType::Vec4);

    // -------------- For texture/material use and preview --------------
    u_uvTransform = bgfx::createUniform("u_uvTransform", bgfx::UniformType::Vec4);
    u_albedoFactor = bgfx::createUniform("u_albedoFactor", bgfx::UniformType::Vec4);


    // SHADER NOISE TEXTURE
    //bgfx::TextureHandle noiseTexture = loadTextureDDS("shaders\\noise1.dds");
    {
        TextureOption noiseTex;

        noiseTex.name = "Noise1(Default)";
        noiseTex.handle = loadTextureDDS("noise textures\\noise1.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise2";
        noiseTex.handle = loadTextureDDS("noise textures\\noise2.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise3";
        noiseTex.handle = loadTextureDDS("noise textures\\noise3.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise4";
        noiseTex.handle = loadTextureDDS("noise textures\\noise4.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise5";
        noiseTex.handle = loadTextureDDS("noise textures\\noise5.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise6";
        noiseTex.handle = loadTextureDDS("noise textures\\noise6.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise7";
        noiseTex.handle = loadTextureDDS("noise textures\\noise7.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise8";
        noiseTex.handle = loadTextureDDS("noise textures\\noise8.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise9";
        noiseTex.handle = loadTextureDDS("noise textures\\noise9.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise10";
        noiseTex.handle = loadTextureDDS("noise textures\\noise10.dds");
        availableNoiseTextures.push_back(noiseTex);

        noiseTex.name = "Noise11";
        noiseTex.handle = loadTextureDDS("noise textures\\noise11.dds");
        availableNoiseTextures.push_back(noiseTex);
    }

    noiseTexture = availableNoiseTextures[0].handle;

    // Texture
    std::vector<TextureOption> availableTextures;
    {
        TextureOption tex;

        // Asphalt 1
        tex.name = "Asphalt1";
        tex.handle = loadTextureDDS("shaders\\Asphalt 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Bark 1
        tex.name = "Bark1";
        tex.handle = loadTextureDDS("shaders\\Bark 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Brick 1
        tex.name = "Brick1";
        tex.handle = loadTextureDDS("shaders\\Brick 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Carpet 1
        tex.name = "Carpet1";
        tex.handle = loadTextureDDS("shaders\\Carpet 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Cobblestone 1
        tex.name = "Cobblestone1";
        tex.handle = loadTextureDDS("shaders\\Cobblestone 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Concrete 1
        tex.name = "Concrete1";
        tex.handle = loadTextureDDS("shaders\\Concrete 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Dirt 1
        tex.name = "Dirt1";
        tex.handle = loadTextureDDS("shaders\\Dirt 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Fabric 1
        tex.name = "Fabric1";
        tex.handle = loadTextureDDS("shaders\\Fabric 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Food 1
        tex.name = "Food1";
        tex.handle = loadTextureDDS("shaders\\Food 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Glass 1
        tex.name = "Glass1";
        tex.handle = loadTextureDDS("shaders\\Glass 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Glass 2
        tex.name = "Glass2";
        tex.handle = loadTextureDDS("shaders\\Glass 2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Grass 1
        tex.name = "Grass1";
        tex.handle = loadTextureDDS("shaders\\Grass 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Grass 2
        tex.name = "Grass2";
        tex.handle = loadTextureDDS("shaders\\Grass 2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Leaves 1
        tex.name = "Leaves1";
        tex.handle = loadTextureDDS("shaders\\Leaves 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Metal 1
        tex.name = "Metal10";
        tex.handle = loadTextureDDS("shaders\\Metal 10.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Paint 1
        tex.name = "Paint1";
        tex.handle = loadTextureDDS("shaders\\Paint 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Rock 1
        tex.name = "Rock1";
        tex.handle = loadTextureDDS("shaders\\Rocks 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Shingles 1
        tex.name = "Shingles1";
        tex.handle = loadTextureDDS("shaders\\Shingles 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Snow 1
        tex.name = "Snow1";
        tex.handle = loadTextureDDS("shaders\\Snow 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Stone 1
        tex.name = "Stone1";
        tex.handle = loadTextureDDS("shaders\\Stone 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Stone 2
        tex.name = "Stone2";
        tex.handle = loadTextureDDS("shaders\\Stone 2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Tile 1
        tex.name = "Tile1";
        tex.handle = loadTextureDDS("shaders\\Tile 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Tile 2
        tex.name = "Tile2";
        tex.handle = loadTextureDDS("shaders\\Tile 2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Wood 1
        tex.name = "Wood1";
        tex.handle = loadTextureDDS("shaders\\Wood 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Wood 2
        tex.name = "Wood2";
        tex.handle = loadTextureDDS("shaders\\Wood 2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Wood 3
        tex.name = "Wood3";
        tex.handle = loadTextureDDS("shaders\\Wood 3.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Wooden Boards 1
        tex.name = "Wooden Boards1";
        tex.handle = loadTextureDDS("shaders\\Wooden Boards 1.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);

        // Wooden Boards 2
        tex.name = "Wooden Boards2";
        tex.handle = loadTextureDDS("shaders\\Wooden Boards 2.dds");
        std::cout << "Loaded texture '" << tex.name << "' with handle: " << tex.handle.idx << std::endl;
        availableTextures.push_back(tex);
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
    bgfx::ShaderHandle vsh = loadShader("shaders\\v_out21.bin");
    bgfx::ShaderHandle fsh = loadShader("shaders\\f_out27.bin");

    bgfx::ProgramHandle defaultProgram = bgfx::createProgram(vsh, fsh, true);

    // Load the debug light shader:
    bgfx::ShaderHandle debugVsh = loadShader("shaders\\v_lightdebug_out1.bin");
    bgfx::ShaderHandle debugFsh = loadShader("shaders\\f_lightdebug_out1.bin");
    bgfx::ProgramHandle lightDebugProgram = bgfx::createProgram(debugVsh, debugFsh, true);
    
    //spawn plane
    spawnInstance(camera, "plane", "plane", vbh_plane, ibh_plane, instances);
    instances.back()->position[0] = 0.0f;
    instances.back()->position[1] = -4.0f;
    instances.back()->position[2] = 0.0f;

    // --- Spawn Cornell Box with Hierarchy ---
    Instance* cornellBox = new Instance(instanceCounter++, "cornell_box", "empty", 8.0f, 0.0f, -5.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
    // Create a walls node (dummy instance without geometry)
    Instance* wallsNode = new Instance(instanceCounter++, "walls", "empty", 0.0f, -1.0f, 0.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
    wallsNode->scale[0] = 0.2f;
    wallsNode->scale[1] = 0.2f;
    wallsNode->scale[2] = 0.2f;

    Instance* floorPlane = new Instance(instanceCounter++, "floor", "plane", 0.0f, -6.0f, 0.0f, vbh_plane, ibh_plane);
    wallsNode->addChild(floorPlane);
    Instance* ceilingPlane = new Instance(instanceCounter++, "ceiling", "plane", 0.0f, 14.0f, 0.0f, vbh_plane, ibh_plane);
    wallsNode->addChild(ceilingPlane);
    Instance* backPlane = new Instance(instanceCounter++, "back", "plane", 0.0f, 4.0f, -10.0f, vbh_plane, ibh_plane);
    backPlane->rotation[0] = 1.57f;
    wallsNode->addChild(backPlane);
    Instance* leftPlane = new Instance(instanceCounter++, "left_wall", "plane", 10.0f, 4.0f, 0.0f, vbh_plane, ibh_plane);
    leftPlane->objectColor[0] = 1.0f; leftPlane->objectColor[1] = 0.0f; leftPlane->objectColor[2] = 0.0f; leftPlane->objectColor[3] = 1.0f;
    leftPlane->rotation[2] = 1.57f;
    wallsNode->addChild(leftPlane);
    Instance* rightPlane = new Instance(instanceCounter++, "right_wall", "plane", -10.0f, 4.0f, 0.0f, vbh_plane, ibh_plane);
    rightPlane->objectColor[0] = 0.0f; rightPlane->objectColor[1] = 1.0f; rightPlane->objectColor[2] = 0.0f; rightPlane->objectColor[3] = 1.0f;
    rightPlane->rotation[2] = 1.57f;
    wallsNode->addChild(rightPlane);

    Instance* innerCube = new Instance(instanceCounter++, "inner_cube", "cube", 0.8f, -1.5f, 0.4f, vbh_cube, ibh_cube);
    innerCube->rotation[1] = 0.2f;
    innerCube->scale[0] = 0.6f;
    innerCube->scale[1] = 0.6f;
    innerCube->scale[2] = 0.6f;
    Instance* innerRectBox = new Instance(instanceCounter++, "inner_rectbox", "cube", -1.0f, -0.7f, 0.4f, vbh_cube, ibh_cube);
    innerRectBox->rotation[1] = -0.3f;
    innerRectBox->scale[0] = 0.6f;
    innerRectBox->scale[1] = 1.5f;
    innerRectBox->scale[2] = 0.6f;
    cornellBox->addChild(wallsNode);
    cornellBox->addChild(innerCube);
    cornellBox->addChild(innerRectBox);
    instances.push_back(cornellBox);

    spawnInstance(camera, "teapot", "teapot", vbh_teapot, ibh_teapot, instances);
    instances.back()->position[0] = 3.0f;
    instances.back()->position[1] = -1.0f;
    instances.back()->position[2] = -5.0f;
    instances.back()->scale[0] *= 0.03f;
    instances.back()->scale[1] *= 0.03f;
    instances.back()->scale[2] *= 0.03f;

    spawnInstance(camera, "bunny", "bunny", vbh_bunny, ibh_bunny, instances);
    instances.back()->position[0] = -1.0f;
    instances.back()->position[1] = -1.0f;
    instances.back()->position[2] = -5.0f;
    instances.back()->scale[0] *= 10.0f;
    instances.back()->scale[1] *= 10.0f;
    instances.back()->scale[2] *= 10.0f;

    spawnInstance(camera, "lucy", "lucy", vbh_lucy, ibh_lucy, instances);
    instances.back()->position[0] = -4.0f;
    instances.back()->position[1] = -1.0f;
    instances.back()->position[2] = -5.0f;
    instances.back()->scale[0] *= 0.01f;
    instances.back()->scale[1] *= 0.01f;
    instances.back()->scale[2] *= 0.01f;
    
    Logger::GetInstance();

    static bgfx::FrameBufferHandle g_frameBuffer = BGFX_INVALID_HANDLE;
    static bgfx::TextureHandle g_frameBufferTex = BGFX_INVALID_HANDLE;

    ImGuiWindowFlags menu_flags = 0;
    menu_flags |= ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBackground;

    bool takingScreenshot = false;

    //MAIN LOOP
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGuiViewport* viewport = ImGui::GetMainViewport();

        if (!takingScreenshot)
        {
            //imgui loop
            ImGui_ImplGlfw_NewFrame();
            ImGui_Implbgfx_NewFrame();
            ImGui::NewFrame();

            //transformation gizmo
            ImGuizmo::BeginFrame();


            ImGuiID dockspace_id = viewport->ID;
            ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

            //for viewporting
            /*
            // Set up a full-screen window
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags docking_window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            ImGui::Begin("DockSpace Window", nullptr, docking_window_flags);
            ImGui::PopStyleVar(3);

            ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            ImGui::End();*/

            ImGui::Begin("MenuBar", p_open, menu_flags);
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File", true))
                {
                    if (ImGui::MenuItem("Open.."))
                    {
                        //std::string loadFilePath = openFileDialog(false); // Open load dialog
                        //if (!loadFilePath.empty())
                        //    loadSceneText(loadFilePath, instances, availableTextures);
                        importedObjMap = loadSceneFromFile(instances, availableTextures, bufferMap);
                    }
                    if (ImGui::MenuItem("Save", "Ctrl+S"))
                    {
                        //std::string saveFilePath = openFileDialog(true); // Open save dialog
                        //if (!saveFilePath.empty())
                        //    saveScene(saveFilePath, instances, availableTextures);
                        saveSceneToFile(instances, availableTextures, importedObjMap);
                    }
                    // ========================
                    // UPDATED IMPORT MENU CODE
                    // ========================

                    /*
                    Inside your ImGui File menu, replace the old OBJ import code with the following:
                    This code:
                      1. Opens a file dialog.
                      2. Loads all meshes from the file (with centering, as done in loadImportedMeshes).
                      3. Creates an empty parent instance (a grouping node) at the center.
                      4. For each imported mesh, creates vertex/index buffers and spawns a child instance.
                      5. Adds each child to the empty parent so that scaling the parent scales all children.
                    */
                    if (ImGui::MenuItem("Import OBJ"))
                    {
                        const char* modelFilter =
                            "All 3D Models\0*.obj;*.fbx;*.dae;*.gltf;*.glb;*.ply;*.stl;*.3ds\0"
                            "OBJ Files (*.obj)\0*.obj\0"
                            "FBX Files (*.fbx)\0*.fbx\0"
                            "COLLADA Files (*.dae)\0*.dae\0"
                            "glTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0"
                            "PLY Files (*.ply)\0*.ply\0"
                            "STL Files (*.stl)\0*.stl\0"
                            "3DS Files (*.3ds)\0*.3ds\0"
                            "All Files (*.*)\0*.*\0";

                        std::string absPath = OpenFileDialog(glfwGetWin32Window(window), modelFilter);
                        std::string relPath = GetRelativePath(absPath);
                        std::string normalizedRelPath = ConvertBackslashesToForward(relPath);
                        std::cout << "filePath: " << normalizedRelPath << std::endl;

                        if (!normalizedRelPath.empty())
                        {
                            // Load all meshes (with textures) using the updated importer.
                            std::vector<ImportedMesh> importedMeshes = loadImportedMeshes(normalizedRelPath);
                            std::string fileName = fs::path(normalizedRelPath).stem().string();

                            // Compute overall group center by averaging each mesh's global translation.
                            aiVector3D groupCenter(0.0f, 0.0f, 0.0f);
                            for (const auto& impMesh : importedMeshes) {
                                groupCenter.x += impMesh.transform.a4;
                                groupCenter.y += impMesh.transform.b4;
                                groupCenter.z += impMesh.transform.c4;
                            }
                            if (!importedMeshes.empty()) {
                                groupCenter.x /= importedMeshes.size();
                                groupCenter.y /= importedMeshes.size();
                                groupCenter.z /= importedMeshes.size();
                            }

                            // Create an empty parent instance at the overall group center.
                            Instance* parentInstance = new Instance(instanceCounter++, fileName + "_group", "empty",
                                groupCenter.x, groupCenter.y, groupCenter.z,
                                BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
                            instances.push_back(parentInstance);

                            // For each imported mesh, create buffers and spawn a child instance.
                            for (size_t i = 0; i < importedMeshes.size(); ++i)
                            {
                                bgfx::VertexBufferHandle vbh_imported;
                                bgfx::IndexBufferHandle ibh_imported;
                                createMeshBuffers(importedMeshes[i].meshData, vbh_imported, ibh_imported);

                                Instance* childInst = new Instance(instanceCounter++, fileName + "_" + std::to_string(i),
                                    fileName + "_" + std::to_string(i), 0.0f, 0.0f, 0.0f,
                                    vbh_imported, ibh_imported);

                                // Decompose the imported mesh's transform.
                                aiVector3D scaling, position;
                                aiQuaternion rotation;
                                importedMeshes[i].transform.Decompose(scaling, rotation, position);
                                // Set the child's position relative to the parent (group center).
                                childInst->position[0] = position.x - groupCenter.x;
                                childInst->position[1] = position.y - groupCenter.y;
                                childInst->position[2] = position.z - groupCenter.z;
                                // For simplicity, we leave rotation at zero or convert the quaternion if desired.
                                childInst->rotation[0] = childInst->rotation[1] = childInst->rotation[2] = 0.0f;
                                childInst->scale[0] = scaling.x;
                                childInst->scale[1] = scaling.y;
                                childInst->scale[2] = scaling.z;

                                // *** NEW: Assign the diffuse texture from the imported mesh ***
                                childInst->diffuseTexture = importedMeshes[i].diffuseTexture;

                                // Add this mesh as a child of the empty parent.
                                parentInstance->addChild(childInst);
                            }

                            std::cout << "Imported OBJ spawned with " << importedMeshes.size()
                                << " mesh(es) grouped under " << fileName << "_group" << std::endl;
                            importedObjMap[fileName] = normalizedRelPath;
                        }
                    }
                    if (ImGui::MenuItem("Import Texture"))
                    {
                        // Open a file dialog to select a texture file.
                        std::string texFilePath = openFileDialog(false);
                        if (!texFilePath.empty())
                        {
                            // Optionally, convert backslashes to forward slashes.
                            std::string normalizedPath = ConvertBackslashesToForward(texFilePath);
                            std::cout << "Importing texture from: " << normalizedPath << std::endl;
                            // Load the texture (currently only DDS files are supported).
                            bgfx::TextureHandle newTexture = loadTextureFile(normalizedPath.c_str());
                            if (newTexture.idx != bgfx::kInvalidHandle)
                            {
                                // Create a TextureOption entry for the new texture.
                                TextureOption texOpt;
                                texOpt.name = fs::path(normalizedPath).stem().string();
                                texOpt.handle = newTexture;
                                availableTextures.push_back(texOpt);
                                std::cout << "Texture '" << texOpt.name << "' imported, handle: "
                                    << texOpt.handle.idx << std::endl;
                            }
                            else
                            {
                                std::cout << "Failed to load texture." << std::endl;
                            }
                        }
                    }
                    if (ImGui::MenuItem("Exit"))
                    {
                        glfwSetWindowShouldClose(window, true);
                    }

                    //if (ImGui::MenuItem("Close", "Ctrl+W")) { /* Do stuff */ }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Add"))
                {
                    if (ImGui::BeginMenu("Objects"))
                    {
                        if (ImGui::MenuItem("Cube"))
                        {
                            spawnInstanceAtCenter("cube", "cube", vbh_cube, ibh_cube, instances);
                            std::cout << "Cube spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Capsule"))
                        {
                            spawnInstanceAtCenter("capsule", "capsule", vbh_capsule, ibh_capsule, instances);
                            std::cout << "Capsule spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Cylinder"))
                        {
                            spawnInstanceAtCenter("cylinder", "cylinder", vbh_cylinder, ibh_cylinder, instances);
                            std::cout << "Cylinder spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Sphere"))
                        {
                            spawnInstanceAtCenter("sphere", "sphere", vbh_sphere, ibh_sphere, instances);
                            std::cout << "Sphere spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Plane"))
                        {
                            spawnInstanceAtCenter("plane", "plane", vbh_plane, ibh_plane, instances);
                            std::cout << "Plane spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Suzanne"))
                        {
                            spawnInstanceAtCenter("mesh", "mesh", vbh_mesh, ibh_mesh, instances);
                            std::cout << "Suzanne spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Cornell Box"))
                        {
                            float x = 0.0f;
                            float y = 0.0f;
                            float z = 0.0f;
                            // --- Spawn Cornell Box with Hierarchy ---
                            Instance* cornellBox = new Instance(instanceCounter++, "cornell_box", "empty", x, y, z, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
                            // Create a walls node (dummy instance without geometry)
                            Instance* wallsNode = new Instance(instanceCounter++, "walls", "empty", 0.0f, -1.0f, 0.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
                            wallsNode->scale[0] = 0.2f;
                            wallsNode->scale[1] = 0.2f;
                            wallsNode->scale[2] = 0.2f;

                            Instance* floorPlane = new Instance(instanceCounter++, "floor", "plane", 0.0f, -6.0f, 0.0f, vbh_plane, ibh_plane);
                            wallsNode->addChild(floorPlane);
                            Instance* ceilingPlane = new Instance(instanceCounter++, "ceiling", "plane", 0.0f, 14.0f, 0.0f, vbh_plane, ibh_plane);
                            wallsNode->addChild(ceilingPlane);
                            Instance* backPlane = new Instance(instanceCounter++, "back", "plane", 0.0f, 4.0f, -10.0f, vbh_plane, ibh_plane);
                            backPlane->rotation[0] = 1.57f;
                            wallsNode->addChild(backPlane);
                            Instance* leftPlane = new Instance(instanceCounter++, "left_wall", "plane", 10.0f, 4.0f, 0.0f, vbh_plane, ibh_plane);
                            leftPlane->objectColor[0] = 1.0f; leftPlane->objectColor[1] = 0.0f; leftPlane->objectColor[2] = 0.0f; leftPlane->objectColor[3] = 1.0f;
                            leftPlane->rotation[2] = 1.57f;
                            wallsNode->addChild(leftPlane);
                            Instance* rightPlane = new Instance(instanceCounter++, "right_wall", "plane", -10.0f, 4.0f, 0.0f, vbh_plane, ibh_plane);
                            rightPlane->objectColor[0] = 0.0f; rightPlane->objectColor[1] = 1.0f; rightPlane->objectColor[2] = 0.0f; rightPlane->objectColor[3] = 1.0f;
                            rightPlane->rotation[2] = 1.57f;
                            wallsNode->addChild(rightPlane);

                            Instance* innerCube = new Instance(instanceCounter++, "inner_cube", "cube", 0.8f, -1.5f, 0.4f, vbh_cube, ibh_cube);
                            innerCube->rotation[1] = 0.2f;
                            innerCube->scale[0] = 0.6f;
                            innerCube->scale[1] = 0.6f;
                            innerCube->scale[2] = 0.6f;
                            Instance* innerRectBox = new Instance(instanceCounter++, "inner_rectbox", "cube", -1.0f, -0.7f, 0.4f, vbh_cube, ibh_cube);
                            innerRectBox->rotation[1] = -0.3f;
                            innerRectBox->scale[0] = 0.6f;
                            innerRectBox->scale[1] = 1.5f;
                            innerRectBox->scale[2] = 0.6f;
                            cornellBox->addChild(wallsNode);
                            cornellBox->addChild(innerCube);
                            cornellBox->addChild(innerRectBox);
                            instances.push_back(cornellBox);
                            std::cout << "Cornell Box spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Teapot"))
                        {
                            spawnInstanceAtCenter("teapot", "teapot", vbh_teapot, ibh_teapot, instances);
                            std::cout << "Teapot spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Bunny"))
                        {
                            spawnInstanceAtCenter("bunny", "bunny", vbh_bunny, ibh_bunny, instances);
                            std::cout << "Bunny spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Lucy"))
                        {
                            spawnInstanceAtCenter("lucy", "lucy", vbh_lucy, ibh_lucy, instances);
                            std::cout << "Lucy spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Empty"))
                        {
                            spawnInstanceAtCenter("empty", "empty", BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, instances);
                            std::cout << "Empty object spawned" << std::endl;
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Lights"))
                    {
                        if (ImGui::MenuItem("Regular Light"))
                        {
                            spawnLight(camera, vbh_sphere, ibh_sphere, vbh_cone, ibh_cone, instances);
                            std::cout << "Light spawned" << std::endl;
                        }
                        if (ImGui::MenuItem("Rotating Spotlight"))
                        {
                            float radius = 5.0f;
                            float height = 5.0f;
                            Instance* rotatingLight = new Instance(instanceCounter++, "rotating_light", "light",
                                radius, height, 0.0f, vbh_cone, ibh_cone);
                            rotatingLight->isLight = true;
                            rotatingLight->lightProps.type = LightType::Spot;
                            rotatingLight->lightProps.intensity = 2.0f;
                            rotatingLight->lightProps.range = 20.0f;
                            rotatingLight->lightProps.coneAngle = 0.5f;  // Narrower beam
                            instances.push_back(rotatingLight);
                            std::cout << "Rotating spotlight added" << std::endl;
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItem("Import OBJ"))
                    {
                        const char* modelFilter =
                            "All 3D Models\0*.obj;*.fbx;*.dae;*.gltf;*.glb;*.ply;*.stl;*.3ds\0"
                            "OBJ Files (*.obj)\0*.obj\0"
                            "FBX Files (*.fbx)\0*.fbx\0"
                            "COLLADA Files (*.dae)\0*.dae\0"
                            "glTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0"
                            "PLY Files (*.ply)\0*.ply\0"
                            "STL Files (*.stl)\0*.stl\0"
                            "3DS Files (*.3ds)\0*.3ds\0"
                            "All Files (*.*)\0*.*\0";

                        std::string absPath = OpenFileDialog(glfwGetWin32Window(window), modelFilter);
                        std::string relPath = GetRelativePath(absPath);
                        std::string normalizedRelPath = ConvertBackslashesToForward(relPath);
                        std::cout << "filePath: " << normalizedRelPath << std::endl;

                        if (!normalizedRelPath.empty())
                        {
                            // Load all meshes (with textures) using the updated importer.
                            std::vector<ImportedMesh> importedMeshes = loadImportedMeshes(normalizedRelPath);
                            std::string fileName = fs::path(normalizedRelPath).stem().string();

                            // Compute overall group center by averaging each mesh's global translation.
                            aiVector3D groupCenter(0.0f, 0.0f, 0.0f);
                            for (const auto& impMesh : importedMeshes) {
                                groupCenter.x += impMesh.transform.a4;
                                groupCenter.y += impMesh.transform.b4;
                                groupCenter.z += impMesh.transform.c4;
                            }
                            if (!importedMeshes.empty()) {
                                groupCenter.x /= importedMeshes.size();
                                groupCenter.y /= importedMeshes.size();
                                groupCenter.z /= importedMeshes.size();
                            }

                            // Create an empty parent instance at the overall group center.
                            Instance* parentInstance = new Instance(instanceCounter++, fileName + "_group", "empty",
                                groupCenter.x, groupCenter.y, groupCenter.z,
                                BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
                            instances.push_back(parentInstance);

                            // For each imported mesh, create buffers and spawn a child instance.
                            for (size_t i = 0; i < importedMeshes.size(); ++i)
                            {
                                bgfx::VertexBufferHandle vbh_imported;
                                bgfx::IndexBufferHandle ibh_imported;
                                createMeshBuffers(importedMeshes[i].meshData, vbh_imported, ibh_imported);

                                Instance* childInst = new Instance(instanceCounter++, fileName + "_" + std::to_string(i),
                                    fileName + "_" + std::to_string(i), 0.0f, 0.0f, 0.0f,
                                    vbh_imported, ibh_imported);

                                // Decompose the imported mesh's transform.
                                aiVector3D scaling, position;
                                aiQuaternion rotation;
                                importedMeshes[i].transform.Decompose(scaling, rotation, position);
                                // Set the child's position relative to the parent (group center).
                                childInst->position[0] = position.x - groupCenter.x;
                                childInst->position[1] = position.y - groupCenter.y;
                                childInst->position[2] = position.z - groupCenter.z;
                                // For simplicity, we leave rotation at zero or convert the quaternion if desired.
                                childInst->rotation[0] = childInst->rotation[1] = childInst->rotation[2] = 0.0f;
                                childInst->scale[0] = scaling.x;
                                childInst->scale[1] = scaling.y;
                                childInst->scale[2] = scaling.z;

                                // *** NEW: Assign the diffuse texture from the imported mesh ***
                                childInst->diffuseTexture = importedMeshes[i].diffuseTexture;

                                // Add this mesh as a child of the empty parent.
                                parentInstance->addChild(childInst);
                            }

                            std::cout << "Imported OBJ spawned with " << importedMeshes.size()
                                << " mesh(es) grouped under " << fileName << "_group" << std::endl;
                            importedObjMap[fileName] = normalizedRelPath;
                        }
                    }

                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Edit"))
                {
                    if (ImGui::MenuItem("Undo", "WIP"))
                    {
                        //undo
                    }
                    if (ImGui::MenuItem("Redo", "WIP"))
                    {
                        //redo
                    }
                    if (ImGui::MenuItem("Delete Last Instance"))
                    {
                        Instance* inst = instances.back();
                        instances.pop_back();
                        //instanceCounter--;
                        delete inst;
                        selectedInstance = nullptr;
                        std::cout << "Last Instance removed" << std::endl;
                    }
                    if (ImGui::MenuItem("Clear All Instances"))
                    {
                        for (Instance* inst : instances)
                        {
                            delete inst;
                        }
                        instances.clear();
                        selectedInstance = nullptr;
                        std::cout << "All Instances cleared" << std::endl;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }
            ImGui::End();

            //IMGUI WINDOW FOR CONTROLS
            //FOR REFERENCE USE THIS: https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            ImGui::Begin("Inspector", p_open, window_flags);

            // If an instance is selected, show its transform controls.
            if (selectedInstance)
            {
                int width = static_cast<int>(viewport->Size.x);
                int height = static_cast<int>(viewport->Size.y);
                float view[16];
                bx::mtxLookAt(view, camera.position, bx::add(camera.position, camera.front), camera.up);

                float proj[16];
                bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);

                DrawGizmoForSelected(selectedInstance, view, proj);

                ImGui::SetNextItemOpen(true, ImGuiCond_Once);//collapsing header set to open initially
                if (ImGui::CollapsingHeader("Transform Controls/Gizmo"))
                {
                    ImGui::Separator();
                    ImGui::Text("Selected: %s", selectedInstance->name.c_str());
                    if (ImGui::RadioButton("Translate", currentGizmoOperation == ImGuizmo::TRANSLATE))
                        currentGizmoOperation = ImGuizmo::TRANSLATE;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Rotate", currentGizmoOperation == ImGuizmo::ROTATE))
                        currentGizmoOperation = ImGuizmo::ROTATE;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Scale", currentGizmoOperation == ImGuizmo::SCALE))
                        currentGizmoOperation = ImGuizmo::SCALE;

                    ImGui::DragFloat3("Translation", selectedInstance->position, 0.1f);
                    ImGui::DragFloat3("Rotation (radians)", selectedInstance->rotation, 0.1f);
                    ImGui::DragFloat3("Scale", selectedInstance->scale, 0.1f);

                    if (currentGizmoOperation != ImGuizmo::SCALE) {
                        if (ImGui::RadioButton("World", currentGizmoMode == ImGuizmo::WORLD))
                            currentGizmoMode = ImGuizmo::WORLD;
                        ImGui::SameLine();
                        if (ImGui::RadioButton("Local", currentGizmoMode == ImGuizmo::LOCAL))
                            currentGizmoMode = ImGuizmo::LOCAL;
                    }
                    ImGui::Separator();
                    ImGui::Text("Snapping Options:");
                    ImGui::BulletText("Hold CTRL while rotating to snap to 90°");
                    ImGui::BulletText("Hold CTRL while translating for 0.5 unit snapping");
                    ImGui::BulletText("Hold CTRL while scaling for 0.5 unit snapping");

                    // Display current snap status
                    bool isSnapping = ImGui::GetIO().KeyCtrl;
                    ImGui::TextColored(
                        isSnapping ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                        isSnapping ? "Snapping ENABLED (CTRL held)" : "Snapping disabled (hold CTRL to enable)"
                    );

                    if (selectedInstance->isLight)
                    {
                        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                        ImGui::SetNextItemOpen(true, ImGuiCond_Once);//collapsing header set to open initially
                        if (ImGui::CollapsingHeader("Light Settings"))
                        {
                            ImGui::Separator();
                            ImGui::Text("Light Properties:");
                            const char* lightTypes[] = { "Directional", "Point", "Spot" };
                            int currentType = static_cast<int>(selectedInstance->lightProps.type);
                            if (ImGui::Combo("Light Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
                            {
                                selectedInstance->lightProps.type = static_cast<LightType>(currentType);

                                if (selectedInstance->lightProps.type == LightType::Point)
                                {
                                    selectedInstance->vertexBuffer = vbh_sphere;
                                    selectedInstance->indexBuffer = ibh_sphere;
                                }
                                else if (selectedInstance->lightProps.type == LightType::Spot ||
                                    selectedInstance->lightProps.type == LightType::Directional)
                                {
                                    selectedInstance->vertexBuffer = vbh_cone;
                                    selectedInstance->indexBuffer = ibh_cone;
                                }
                            }
                            if (selectedInstance->lightProps.type == LightType::Directional ||
                                selectedInstance->lightProps.type == LightType::Spot)
                            {
                                ImGui::DragFloat3("Light Direction", selectedInstance->lightProps.direction, 0.1f);
                            }
                            if (selectedInstance->lightProps.type == LightType::Point ||
                                selectedInstance->lightProps.type == LightType::Spot)
                            {
                                ImGui::DragFloat3("Light Position", selectedInstance->position, 0.1f);
                                ImGui::DragFloat("Range", &selectedInstance->lightProps.range, 0.1f, 0.0f, 100.0f);
                            }
                            ImGui::ColorEdit4("Light Color", selectedInstance->lightProps.color);
                            ImGui::DragFloat("Intensity", &selectedInstance->lightProps.intensity, 0.1f, 0.0f, 10.0f);
                            if (selectedInstance->lightProps.type == LightType::Spot)
                            {
                                ImGui::DragFloat("Cone Angle", &selectedInstance->lightProps.coneAngle, 0.1f, 0.0f, 3.14f);
                            }

                            if (selectedInstance->lightProps.type == LightType::Point ||
                                selectedInstance->lightProps.type == LightType::Spot)
                            {
                                ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                                if (ImGui::CollapsingHeader("Light Animation Settings"))
                                {
                                    ImGui::Separator();
                                    // Animate Light control:
                                    bool animEnabled = selectedInstance->lightAnim.enabled;
                                    if (ImGui::Checkbox("Animate Light", &animEnabled))
                                    {
                                        // When the user toggles the checkbox, update the instance's animation state.
                                        selectedInstance->lightAnim.enabled = animEnabled;
                                        // If animation has just been enabled, set the base position to the current position.
                                        if (animEnabled)
                                        {
                                            for (int i = 0; i < 3; i++)
                                            {
                                                selectedInstance->basePosition[i] = selectedInstance->position[i];
                                            }
                                        }
                                    }
                                    ImGui::DragFloat3("Animation Amplitude", selectedInstance->lightAnim.amplitude, 0.1f, 0.0f, 10.0f);
                                    ImGui::DragFloat3("Animation Frequency", selectedInstance->lightAnim.frequency, 0.1f, 0.0f, 10.0f);
                                    ImGui::DragFloat3("Animation Phase", selectedInstance->lightAnim.phase, 0.1f, 0.0f, 10.0f);
                                }
                            }

                            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                            // Add a checkbox to show or hide the debug visual of the light.
                            bool debugVisible = selectedInstance->showDebugVisual;
                            if (ImGui::Checkbox("Show Light Debug Visual", &debugVisible))
                            {
                                selectedInstance->showDebugVisual = debugVisible;
                            }
                            ImGui::Separator();
                            if (selectedInstance->name.find("rotating_light") != std::string::npos) {
                                ImGui::Text("Rotating Light Properties:");
                                ImGui::DragFloat("Radius", &selectedInstance->radius, 0.1f, 0.0f, 100.0f);
                                ImGui::DragFloat("Center X", &selectedInstance->centerX, 0.1f);
                                ImGui::DragFloat("Center Z", &selectedInstance->centerZ, 0.1f);
                                ImGui::DragFloat("Rotation Speed", &selectedInstance->rotationSpeed, 0.1f);
                            }

                        }
                    }
                    else {
                        //Object color Selection
                        ImGui::Separator();
                        ImGui::Spacing(); ImGui::Spacing();
                        ImGui::ColorEdit3("Object Color", selectedInstance->objectColor);
                        ImGui::Spacing(); ImGui::Spacing();
                        ImGui::Separator();
                        // --- Texture/Material Editor ---
                        ImGui::SetNextItemOpen(true, ImGuiCond_Once);//collapsing header set to open initially
                        if (ImGui::CollapsingHeader("Material Editor")) {
                            ImGui::Spacing(); ImGui::Spacing();

                            ImGui::Text("Available Material:"); ImGui::Spacing(); ImGui::Spacing();
                            ImGui::BeginChild("TextureSelection", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);
                            for (size_t i = 0; i < availableTextures.size(); i++) {
                                // Convert your BGFX texture handle to an ImGui texture ID.
                                ImTextureID texID = static_cast<ImTextureID>(static_cast<uintptr_t>(availableTextures[i].handle.idx));
                                // Display each texture as an image button (64x64 pixels).
                                if (ImGui::ImageButton(std::to_string(i).c_str(), texID, ImVec2(64, 64)))
                                {
                                    // When clicked, update your selected texture.
                                    // For example, assign it to the currently selected instance.
                                    selectedInstance->diffuseTexture = availableTextures[i].handle;
                                }
                                if (i < availableTextures.size() - 1)
                                {
                                    // Use SameLine to arrange buttons horizontally.
                                    ImGui::SameLine();
                                }
                            }

                            ImGui::EndChild();
                            // Add a button to clear the selected texture.
                            if (ImGui::Button("Clear Texture"))
                            {
                                selectedInstance->diffuseTexture = BGFX_INVALID_HANDLE;

                                // Reset material parameters to defaults:
                                selectedInstance->material.tiling[0] = 1.0f;
                                selectedInstance->material.tiling[1] = 1.0f;
                                selectedInstance->material.offset[0] = 0.0f;
                                selectedInstance->material.offset[1] = 0.0f;
                                selectedInstance->material.albedo[0] = 1.0f;
                                selectedInstance->material.albedo[1] = 1.0f;
                                selectedInstance->material.albedo[2] = 1.0f;
                                selectedInstance->material.albedo[3] = 1.0f;
                            }

                            ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

                            if (selectedInstance->diffuseTexture.idx != bgfx::kInvalidHandle) {
                                ImGui::Text("Material Parameters:");
                                // Let the user edit the UV tiling.
                                ImGui::DragFloat2("Tiling", selectedInstance->material.tiling, 0.01f, 0.0f, 10.0f);
                                // Let the user edit the UV offset.
                                ImGui::DragFloat2("Offset", selectedInstance->material.offset, 0.01f, -10.0f, 10.0f);
                                // Let the user edit the albedo (color tint).
                                ImGui::ColorEdit4("Albedo", selectedInstance->material.albedo);

                                ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();
                                ImGui::Text("Raw Material Preview:");

                                ImTextureID texID = static_cast<ImTextureID>(static_cast<uintptr_t>(selectedInstance->diffuseTexture.idx));
                                ImGui::Image(texID, ImVec2(256, 256));
                                ImGui::Separator();
                            }
                            else {
                                //ImGui::Text("No texture applied.");
                            }

                        }

                    }
                    //ImGui::Separator();
                    // You can add a button to remove the selected instance from the hierarchy.
                    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
                    if (ImGui::Button("Delete Object"))
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
                    bool highlighted = highlightVisible;
                    if (ImGui::Checkbox("Show highlight tint", &highlighted))
                    {
                        highlightVisible = highlighted;
                    }
                }
            }

            ImGui::End();

            Logger::GetInstance().DrawImGuiLogger();

            ImGui::Begin("Object List", p_open, window_flags);
            static int selectedInstanceIndex = -1;

            ImGui::SetNextItemOpen(true, ImGuiCond_Once);//collapsing header set to open initially
            if (ImGui::CollapsingHeader("Object List"))
            {
                // For each top-level instance, show its tree.
                for (Instance* instance : instances)
                {
                    ShowInstanceTree(instance, selectedInstance, instances);

                }

                // Now, show the drop target region for reparenting to top-level.
                ShowTopLevelDropTarget(instances);


            }
            ImGui::End();

            /*
            ImGui::Begin("Cross Hatch Settings", p_open, window_flags);
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);//collapsing header set to open initially
            if (ImGui::CollapsingHeader("Crosshatch Settings"))
            {
                // ColorEdit4 allows you to edit a vec4 (RGBA)
                ImGui::ColorEdit4("Ink Color", inkColor);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Epsilon", &epsilonValue, 0.001f, 0.0f, 0.1f);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Stroke Multiplier", &strokeMultiplier, 0.1f, 0.0f, 10.0f);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Line Angle 1", &lineAngle1, 0.1f, 0.0f, TAU);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Line Angle 2", &lineAngle2, 0.1f, 0.0f, TAU);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Pattern Scale", &patternScale, 0.1f, 0.1f, 10.0f);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Line Thickness", &lineThickness, 0.1f, 0.1f, 10.0f);
                ImGui::SetNextItemWidth(100);
                ImGui::DragFloat("Line Transparency", &transparencyValue, 0.01f, 0.0f, 1.0f);
            }
            ImGui::End();
            */

            ImGui::Begin("Info", p_open, window_flags);
            ImGui::Text("Crosshatch Editor Demo Build");
            ImGui::Text("Press F1 to toggle bgfx stats");
            ImGui::Text("FPS: %.1f ", ImGui::GetIO().Framerate);
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Separator();
            ImGui::Text("Rendered Instances: %d", instances.size());
            ImGui::Text("Selected Instance: %s", selectedInstance ? selectedInstance->name.c_str() : "None");
            ImGui::Text("Selected Instance ID: %d", selectedInstance ? selectedInstance->id : -1);
            ImGui::Text("Selected Instance Parent: %s", selectedInstance && selectedInstance->parent ? selectedInstance->parent->name.c_str() : "None");
            ImGui::Text("Selected Instance Children: %d", selectedInstance ? selectedInstance->children.size() : 0);
            ImGui::Separator();
            ImGui::Text("Camera Position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
            //ImGui::Text("Frame: % 7.3f[ms]", 1000.0f / bgfx::getStats()->cpuTimeFrame);
            ImGui::Text("");
            ImGui::End();

            ImGui::Begin("Controls", p_open, window_flags);
            ImGui::Text("Controls:");
            ImGui::Text("WASD - Move Camera");
            ImGui::Text("Mouse - Look Around");
            ImGui::Text("Shift - Move Down");
            ImGui::Text("Space - Move Up");
            ImGui::Text("Left Click - Select Object");
            ImGui::Text("Right Click - Detach Mouse from Camera");
            ImGui::Text("C - Randomize Light Color");
            ImGui::Text("X - Reset Light Color");
            ImGui::Text("V - Randomize Light Direction");
            ImGui::Text("Z - Reset Light Direction");
            ImGui::Text("F1 - Toggle bgfx stats");
            ImGui::Text("F2 - Disable/Enable UI");
            ImGui::Text("F3 - Take Screenshot");
            ImGui::End();


            ImGui::Begin("Crosshatch Shader Settings");
            ImGui::Checkbox("Use Global Crosshatch Shader Settings", &useGlobalCrosshatchSettings);
            if (useGlobalCrosshatchSettings) {
                const char* modeItems[] = { "Crosshatch Ver 1.0", "Crosshatch Ver 1.1", "Crosshatch Ver 1.2", "Simple Lighting" };
                ImGui::Combo("Shader Mode", &crosshatchMode, modeItems, IM_ARRAYSIZE(modeItems));
                ImGui::Spacing(); ImGui::Spacing();
                // --- Show controls depending on the mode ---
                if (crosshatchMode == 0)
                {
                    ImGui::Text("Crosshatch Ver 1.0 Settings:");
                    ImGui::ColorEdit4("Hatch Color", inkColor);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Smoothness", &epsilonValue, 0.001f, 0.0f, 0.1f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Density", &strokeMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Primary Hatch Angle", &lineAngle1, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Secondary Hatch Angle", &lineAngle2, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Scale", &patternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Weight", &lineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Opacity", &transparencyValue, 0.01f, 0.0f, 1.0f);
                }
                else if (crosshatchMode == 1)
                {
                    ImGui::Text("Crosshatch Ver 1.1 Settings:");
                    ImGui::ColorEdit4("Hatch Color", inkColor);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Smoothness", &epsilonValue, 0.001f, 0.0f, 0.1f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Density", &strokeMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Angle", &lineAngle1, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Scale", &patternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Weight", &lineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Opacity", &transparencyValue, 0.01f, 0.0f, 1.0f);
                }
                else if (crosshatchMode == 2)
                {
                    ImGui::Text("Crosshatch Ver 1.2 Settings:");
                    ImGui::ColorEdit4("Hatch Color", inkColor);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Line Smoothness", &epsilonValue, 0.001f, 0.0f, 0.1f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Density", &strokeMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Angle", &lineAngle1, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Scale", &patternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Weight", &lineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    // Inner layer settings:
                    ImGui::DragFloat("Inner Hatch Scale", &layerPatternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Inner Hatch Density", &layerStrokeMult, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Inner Hatch Angle", &layerAngle, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Inner Hatch Weight", &layerLineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Opacity", &transparencyValue, 0.01f, 0.0f, 1.0f);
                }
                else if (crosshatchMode == 3)
                {
                    ImGui::Text("Simple Lighting (No Crosshatch)");
                }
                // New: noise texture selection
                if (!availableNoiseTextures.empty() && crosshatchMode != 3)
                {
                    // Automatically update currentNoiseIndex based on the instance's noise texture.
                    bool found = false;
                    for (int i = 0; i < (int)availableNoiseTextures.size(); i++)
                    {
                        if (availableNoiseTextures[i].handle.idx == noiseTexture.idx)
                        {
                            globalCurrentNoiseIndex = i;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        // If the instance doesn't have a valid noise texture, default to index 0.
                        globalCurrentNoiseIndex = 0;
                        noiseTexture = availableNoiseTextures[0].handle;
                    }

                    // Build an array of c-strings from the names in availableNoiseTextures
                    std::vector<const char*> noiseNames;
                    noiseNames.reserve(availableNoiseTextures.size());
                    for (auto& n : availableNoiseTextures)
                    {
                        noiseNames.push_back(n.name.c_str());
                    }

                    ImGui::Spacing(); ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing(); ImGui::Spacing();
                    // Let user pick which noise texture to use
                    if (ImGui::Combo("Noise Pattern", &globalCurrentNoiseIndex, noiseNames.data(), (int)noiseNames.size()))
                    {
                        noiseTexture = availableNoiseTextures[globalCurrentNoiseIndex].handle;
                    }

                    ImGui::Text("Noise Texture Preview:");
                    if (bgfx::isValid(noiseTexture))
                    {
                        ImTextureID noiseID = (ImTextureID)(uintptr_t)(noiseTexture.idx);
                        ImGui::Image(noiseID, ImVec2(256, 256));
                    }
                    else
                    {
                        ImGui::Text("No valid noise texture selected.");
                    }
                }
            }
            else if (selectedInstance && selectedInstance->isLight == false) {
                const char* modeItems[] = { "Crosshatch Ver 1.0", "Crosshatch Ver 1.1", "Crosshatch Ver 1.2", "Simple Lighting" };
                ImGui::Combo("Shader Mode", &selectedInstance->crosshatchMode, modeItems, IM_ARRAYSIZE(modeItems));
                ImGui::Spacing(); ImGui::Spacing();
                // --- Show controls depending on the mode ---
                if (selectedInstance->crosshatchMode == 0)
                {
                    ImGui::Text("Crosshatch Ver 1.0 Settings:");
                    ImGui::ColorEdit4("Hatch Color", selectedInstance->inkColor);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Smoothness", &selectedInstance->epsilonValue, 0.001f, 0.0f, 0.1f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Density", &selectedInstance->strokeMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Primary Hatch Angle", &selectedInstance->lineAngle1, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Secondary Hatch Angle", &selectedInstance->lineAngle2, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Scale", &selectedInstance->patternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Weight", &selectedInstance->lineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Opacity", &selectedInstance->transparencyValue, 0.01f, 0.0f, 1.0f);
                }
                else if (selectedInstance->crosshatchMode == 1)
                {
                    ImGui::Text("Crosshatch Ver 1.1 Settings:");
                    ImGui::ColorEdit4("Hatch Color", selectedInstance->inkColor);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Smoothness", &selectedInstance->epsilonValue, 0.001f, 0.0f, 0.1f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Density", &selectedInstance->strokeMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Angle", &selectedInstance->lineAngle1, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Scale", &selectedInstance->patternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Line Weight", &selectedInstance->lineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Opacity", &selectedInstance->transparencyValue, 0.01f, 0.0f, 1.0f);
                }
                else if (selectedInstance->crosshatchMode == 2)
                {
                    ImGui::Text("Crosshatch Ver 1.2 Settings:");
                    ImGui::ColorEdit4("Hatch Color", selectedInstance->inkColor);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Line Smoothness", &selectedInstance->epsilonValue, 0.001f, 0.0f, 0.1f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Density", &selectedInstance->strokeMultiplier, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Angle", &selectedInstance->lineAngle1, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Scale", &selectedInstance->patternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Outer Hatch Weight", &selectedInstance->lineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    // Inner layer settings:
                    ImGui::DragFloat("Inner Hatch Scale", &selectedInstance->layerPatternScale, 0.1f, 0.1f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Inner Hatch Density", &selectedInstance->layerStrokeMult, 0.1f, 0.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Inner Hatch Angle", &selectedInstance->layerAngle, 0.1f, 0.0f, TAU);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Inner Hatch Weight", &selectedInstance->layerLineThickness, 0.1f, -10.0f, 10.0f);
                    ImGui::SetNextItemWidth(100);
                    ImGui::DragFloat("Hatch Opacity", &selectedInstance->transparencyValue, 0.01f, 0.0f, 1.0f);
                }
                else if (selectedInstance->crosshatchMode == 3)
                {
                    ImGui::Text("Simple Lighting (No Crosshatch)");
                }

                // New: noise texture selection
                if (!availableNoiseTextures.empty() && selectedInstance->crosshatchMode != 3)
                {
                    // Automatically update currentNoiseIndex based on the instance's noise texture.
                    bool found = false;
                    for (int i = 0; i < (int)availableNoiseTextures.size(); i++)
                    {
                        if (availableNoiseTextures[i].handle.idx == selectedInstance->noiseTexture.idx)
                        {
                            currentNoiseIndex = i;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        // If the instance doesn't have a valid noise texture, default to index 0.
                        currentNoiseIndex = 0;
                        selectedInstance->noiseTexture = availableNoiseTextures[0].handle;
                    }

                    // Build an array of c-strings from the names in availableNoiseTextures
                    std::vector<const char*> noiseNames;
                    noiseNames.reserve(availableNoiseTextures.size());
                    for (auto& n : availableNoiseTextures)
                    {
                        noiseNames.push_back(n.name.c_str());
                    }

                    ImGui::Spacing(); ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing(); ImGui::Spacing();
                    // Let user pick which noise texture to use
                    if (ImGui::Combo("Noise Pattern", &currentNoiseIndex, noiseNames.data(), (int)noiseNames.size()))
                    {
                        selectedInstance->noiseTexture = availableNoiseTextures[currentNoiseIndex].handle;
                    }

                    ImGui::Text("Noise Texture Preview:");
                    if (bgfx::isValid(selectedInstance->noiseTexture))
                    {
                        ImTextureID noiseID = (ImTextureID)(uintptr_t)(selectedInstance->noiseTexture.idx);
                        ImGui::Image(noiseID, ImVec2(256, 256));
                    }
                    else
                    {
                        ImGui::Text("No valid noise texture selected.");
                    }
                }
            }
            ImGui::End();

            ImGui::Render();
            ImGui_Implbgfx_RenderDrawLists(ImGui::GetDrawData());
        }

        //handle inputs
        InputManager::update(camera, 0.016f);

        if (InputManager::isKeyToggled(GLFW_KEY_F2))
        {
            takingScreenshot = !takingScreenshot;
        }

        if (InputManager::isKeyToggled(GLFW_KEY_F3))
        {
            bgfx::requestScreenShot(BGFX_INVALID_HANDLE, "screenshot");
        }


        /*
        //call spawnInstance when key is pressed based on spawnPrimitive value
        if (InputManager::isMouseClicked(GLFW_MOUSE_BUTTON_LEFT) && InputManager::getCursorDisabled())
        {
            if (spawnPrimitive == 0)
            {
                spawnInstance(camera, "cube", "cube", vbh_cube, ibh_cube, instances);
                std::cout << "Cube spawned" << std::endl;
            }
            else if (spawnPrimitive == 1)
            {
                spawnInstance(camera, "capsule", "capsule", vbh_capsule, ibh_capsule, instances);
                std::cout << "Capsule spawned" << std::endl;
            }
            else if (spawnPrimitive == 2)
            {
                spawnInstance(camera, "cylinder", "cylinder", vbh_cylinder, ibh_cylinder, instances);
                std::cout << "Cylinder spawned" << std::endl;
            }
            else if (spawnPrimitive == 3)
            {
                spawnInstance(camera, "sphere", "sphere", vbh_sphere, ibh_sphere, instances);
                std::cout << "Sphere spawned" << std::endl;
            }
            else if (spawnPrimitive == 4)
            {
                spawnInstance(camera, "plane", "plane", vbh_plane, ibh_plane, instances);
                std::cout << "Plane spawned" << std::endl;
            }
            else if (spawnPrimitive == 5)
            {
                spawnInstance(camera, "mesh", "mesh", vbh_mesh, ibh_mesh, instances);
                std::cout << "Test Import spawned" << std::endl;
            }
            else if (spawnPrimitive == 6)
            {
                // --- Spawn Cornell Box with Hierarchy ---
                Instance* cornellBox = new Instance(instanceCounter++, "cornell_box", "empty", 8.0f, 0.0f, -5.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);
                // Create a walls node (dummy instance without geometry)
                Instance* wallsNode = new Instance(instanceCounter++, "walls", "empty", 0.0f, 0.0f, 0.0f, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE);

                Instance* floorPlane = new Instance(instanceCounter++, "floor", "floor", 0.0f, 0.0f, 0.0f, vbh_floor, ibh_floor);
                wallsNode->addChild(floorPlane);
                Instance* ceilingPlane = new Instance(instanceCounter++, "ceiling", "ceiling", 0.0f, 0.0f, 0.0f, vbh_ceiling, ibh_ceiling);
                wallsNode->addChild(ceilingPlane);
                Instance* backPlane = new Instance(instanceCounter++, "back", "back", 0.0f, 0.0f, 0.0f, vbh_back, ibh_back);
                wallsNode->addChild(backPlane);
                Instance* leftPlane = new Instance(instanceCounter++, "left_wall", "left", 0.0f, 0.0f, 0.0f, vbh_left, ibh_left);
                wallsNode->addChild(leftPlane);
                Instance* rightPlane = new Instance(instanceCounter++, "right_wall", "right", 0.0f, 0.0f, 0.0f, vbh_right, ibh_right);
                wallsNode->addChild(rightPlane);

                Instance* innerCube = new Instance(instanceCounter++, "inner_cube", "innerCube", 0.3f, 0.0f, 0.0f, vbh_innerCube, ibh_innerCube);
                Instance* innerRectBox = new Instance(instanceCounter++, "inner_rectbox", "innerCube", 1.1f, 1.0f, -0.9f, vbh_innerCube, ibh_innerCube);
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
                spawnInstance(camera, "teapot", "teapot", vbh_teapot, ibh_teapot, instances);
                std::cout << "Teapot spawned" << std::endl;
            }
            else if (spawnPrimitive == 8)
            {
                spawnInstance(camera, "bunny", "bunny", vbh_bunny, ibh_bunny, instances);
                std::cout << "Bunny spawned" << std::endl;
            }
            else if (spawnPrimitive == 9)
            {
                spawnInstance(camera, "lucy", "lucy", vbh_lucy, ibh_lucy, instances);
                std::cout << "Lucy spawned" << std::endl;
            }
        }
        */


        int width = static_cast<int>(viewport->Size.x);
        int height = static_cast<int>(viewport->Size.y);

        if (width == 0 || height == 0)
        {
            continue;
        }

        // --- Object Picking Pass ---
        // Only execute picking when the left mouse button is clicked and ImGui is not capturing the mouse.
        if (InputManager::isMouseClicked(GLFW_MOUSE_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse)
        {
            if (InputManager::getSkipPickingPass) {
                // Use a dedicated view ID for picking (choose one not used by your normal rendering)
                const uint32_t PICKING_VIEW_ID = 0;
                bgfx::setViewFrameBuffer(PICKING_VIEW_ID, s_pickingFB);
                bgfx::setViewRect(PICKING_VIEW_ID, 0, 0, PICKING_DIM, PICKING_DIM);

                // Use the same camera for the picking pass.
                float view[16];
                bx::mtxLookAt(view, camera.position, bx::add(camera.position, camera.front), camera.up);
                float proj[16];
                bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
                bgfx::setViewTransform(PICKING_VIEW_ID, view, proj);

                // Render each instance with the picking shader.
                for (const Instance* instance : instances)
                {
                    renderInstancePickingRecursive(instance, nullptr, PICKING_VIEW_ID);
                }

                // Blit the picking render target to the CPU-readable texture.
                const uint32_t PICKING_BLIT_VIEW = 2;
                bgfx::blit(PICKING_BLIT_VIEW, s_pickingReadTex, 0, 0, s_pickingRT);
                // Submit a frame to ensure the blit is complete.
                bgfx::frame();

                // Read back the texture data into s_pickingBlitData.
                bgfx::readTexture(s_pickingReadTex, s_pickingBlitData);

                // Detach the picking framebuffer by setting it to BGFX_INVALID_HANDLE.
                bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
                // Reset the viewport to cover the full window.
                bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
                // Reset the view transforms for your normal scene.
                bgfx::setViewTransform(0, view, proj);
                InputManager::toggleSkipPickingPass();
            }
            // Use a dedicated view ID for picking (choose one not used by your normal rendering)
            const uint32_t PICKING_VIEW_ID = 0;
            bgfx::setViewFrameBuffer(PICKING_VIEW_ID, s_pickingFB);
            bgfx::setViewRect(PICKING_VIEW_ID, 0, 0, PICKING_DIM, PICKING_DIM);

            // Use the same camera for the picking pass.
            float view[16];
            bx::mtxLookAt(view, camera.position, bx::add(camera.position, camera.front), camera.up);
            float proj[16];
            bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
            bgfx::setViewTransform(PICKING_VIEW_ID, view, proj);

            // Render each instance with the picking shader.
            for (const Instance* instance : instances)
            {
                renderInstancePickingRecursive(instance, nullptr, PICKING_VIEW_ID);
            }

            // Blit the picking render target to the CPU-readable texture.
            const uint32_t PICKING_BLIT_VIEW = 2;
            bgfx::blit(PICKING_BLIT_VIEW, s_pickingReadTex, 0, 0, s_pickingRT);
            // Submit a frame to ensure the blit is complete.
            bgfx::frame();

            // Read back the texture data into s_pickingBlitData.
            bgfx::readTexture(s_pickingReadTex, s_pickingBlitData);

            // Convert the current mouse position to coordinates in the picking RT.
            int mouseX = static_cast<int>(InputManager::getMouseX());
            int mouseY = static_cast<int>(InputManager::getMouseY());

            // Flip Y coordinate if needed:
            mouseY = height - mouseY;
            int pickX = (mouseX * PICKING_DIM) / width;
            int pickY = (mouseY * PICKING_DIM) / height;

            // Clamp the coordinates.
            pickX = std::max(0, std::min(pickX, PICKING_DIM - 1));
            pickY = std::max(0, std::min(pickY, PICKING_DIM - 1));

            // Read the pixel (RGBA8: 4 bytes per pixel)
            int pixelIndex = (pickY * PICKING_DIM + pickX) * 4;
            uint8_t r = s_pickingBlitData[pixelIndex + 0];

            // Decode the ID from the red channel.
            int pickedID = r;

            // Search through instances to find the one with this ID.
            Instance* pickedInstance = findInstanceById(instances, pickedID);
            if (pickedInstance)
            {
                if (selectedInstance == pickedInstance) {
                    selectedInstance = nullptr;
                }
                else {
                    selectedInstance = pickedInstance;
                    std::cout << "Picked object: " << selectedInstance->name << std::endl;
                }
            }
            // Detach the picking framebuffer by setting it to BGFX_INVALID_HANDLE.
            bgfx::setViewFrameBuffer(0, BGFX_INVALID_HANDLE);
            // Reset the viewport to cover the full window.
            bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
            // Reset the view transforms for your normal scene.
            bgfx::setViewTransform(0, view, proj);
        }

        //if (InputManager::isKeyToggled(GLFW_KEY_BACKSPACE) && !instances.empty())
        //{
        //    Instance* inst = instances.back();
        //    instances.pop_back();
        //    //instanceCounter--;
        //    delete inst;
        //    std::cout << "Last Instance removed" << std::endl;
        //}

        float lightsData[MAX_LIGHTS * 16]; // 16 floats per light.
        int numLights = 0;
        for (const Instance* inst : instances) {
            if (numLights >= MAX_LIGHTS)
                break;
            collectLights(inst, lightsData, numLights);
        }
        // Set u_lights uniform with (numLights * 4) vec4's.
        bgfx::setUniform(u_lights, lightsData, numLights * 4);
        float numLightsArr[4] = { static_cast<float>(numLights), 0, 0, 0 };
        bgfx::setUniform(u_numLights, numLightsArr);

        bgfx::reset(width, height, BGFX_RESET_VSYNC);
        bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));

        float view[16];
        bx::mtxLookAt(view, camera.position, bx::add(camera.position, camera.front), camera.up);

        float proj[16];
        bx::mtxProj(proj, 60.0f, float(width) / float(height), 0.1f, 1000.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(0, view, proj);

        // Set model matrix
        float mtx[16];
        bx::mtxSRT(mtx, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        bgfx::setTransform(mtx);

        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        bgfx::touch(0);


        float viewPos[4] = { camera.position.x, camera.position.y, camera.position.z, 1.0f };

        //bgfx::setUniform(u_lightDir, lightDir);
        //bgfx::setUniform(u_lightColor, lightColor);

        bgfx::setUniform(u_viewPos, viewPos);

        float cameraPos[4] = { camera.position.x, camera.position.y, camera.position.z, 1.0f };
        float epsilon[4] = { 0.02f, 0.0f, 0.0f, 0.0f }; // Pack the epsilon value into the first element; the other three can be 0.

        bgfx::setUniform(u_inkColor, inkColor);
        bgfx::setUniform(u_cameraPos, cameraPos);
        // Set epsilon uniform:
        float epsilonUniform[4] = { epsilonValue, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(u_e, epsilonUniform);
        //bgfx::setTexture(0, u_noiseTex, availableNoiseTextures[currentNoiseIndex].handle); // Bind the noise texture to texture

        // Prepare an array of 4 floats.
        // Set u_params uniform:
        float paramsUniform[4] = { 0.0f, strokeMultiplier, lineAngle1, lineAngle2 };
        bgfx::setUniform(u_params, paramsUniform);

        // Prepare an array of 4 floats.
        float extraParamsUniform[4] = { patternScale, lineThickness, transparencyValue, float(crosshatchMode) };
        // Set the uniform for extra parameters.
        bgfx::setUniform(u_extraParams, extraParamsUniform);


        // Prepare an array of 4 floats.
        float paramsLayerUniform[4] = { layerPatternScale, layerStrokeMult, layerAngle, layerLineThickness };
        // Set the uniform for extra parameters.
        bgfx::setUniform(u_paramsLayer, paramsLayerUniform);

        // Enable stats or debug text
        bgfx::setDebug(s_showStats ? BGFX_DEBUG_STATS : BGFX_DEBUG_TEXT);

        // Calculate delta time
        static float lastFrameTime = 0.0f;
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        // Update rotating lights
        updateRotatingLights(instances, deltaTime);

        const float tintBasic[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
        bgfx::setUniform(u_tint, tintBasic);
        bgfx::submit(0, defaultProgram);

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

            // 1) Build the uvTransform (tilingU, tilingV, offsetU, offsetV).
            float uvTransform[4] =
            {
                instance->material.tiling[0],
                instance->material.tiling[1],
                instance->material.offset[0],
                instance->material.offset[1]
            };
            bgfx::setUniform(u_uvTransform, uvTransform);

            // 2) Albedo factor
            //    (r, g, b, a)
            bgfx::setUniform(u_albedoFactor, instance->material.albedo);

            if (instance->isLight && instance->lightAnim.enabled) {
                float time = static_cast<float>(glfwGetTime());
                // Update each axis (x, y, z) with a sine-based offset.
                for (int i = 0; i < 3; i++) {
                    instance->position[i] = instance->basePosition[i] +
                        instance->lightAnim.amplitude[i] * sin(time * instance->lightAnim.frequency[i] + instance->lightAnim.phase[i]);
                }
            }

            drawInstance(instance, defaultProgram, lightDebugProgram, u_noiseTex, u_diffuseTex, u_objectColor, u_tint, u_inkColor, u_e, u_params, u_extraParams, u_paramsLayer, defaultWhiteTexture, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, instance->objectColor); // your usual shader program
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

        // End frame

        bgfx::frame();


    }
    for (const auto& instance : instances)
    {
        const bgfx::VertexBufferHandle invalidVbh = BGFX_INVALID_HANDLE;
        const bgfx::IndexBufferHandle invalidIbh = BGFX_INVALID_HANDLE;
        // Destroy the vertex and index buffers if they are valid.
        if (instance->vertexBuffer.idx != invalidVbh.idx &&
            instance->indexBuffer.idx != invalidIbh.idx)
        {
            bgfx::destroy(instance->vertexBuffer);
            bgfx::destroy(instance->indexBuffer);
        }
        deleteInstance(instance);
    }

    bgfx::destroy(vbh_plane);
    bgfx::destroy(ibh_plane);
    bgfx::destroy(vbh_sphere);
    bgfx::destroy(ibh_sphere);
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
    bgfx::destroy(defaultProgram);
    bgfx::destroy(lightDebugProgram);
    ImGui_ImplGlfw_Shutdown();

    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
