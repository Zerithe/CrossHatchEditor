#pragma once

#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <bgfx/bgfx.h>
#include <limits>
#include <iostream>
#include <filesystem>
#include "PrimitiveObjects.h"
#include <unordered_map>

struct Vec3 {
    float x, y, z;
};

class MeshLoader {
public:
    struct MeshData {
        std::vector<PosColorVertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct ImportedMesh {
        MeshData meshData;
        aiMatrix4x4 transform; // Global transform (accumulated from the scene hierarchy)
        bgfx::TextureHandle diffuseTexture; // Diffuse texture for this mesh, if available.
    };

    // Load a single mesh from an OBJ file
    static MeshData loadMesh(const std::string& filePath);

    // Create BGFX vertex and index buffers from mesh data
    static void createMeshBuffers(const MeshData& meshData,
        bgfx::VertexBufferHandle& vbh,
        bgfx::IndexBufferHandle& ibh);

    // Load multiple meshes with transforms and textures
    static std::vector<ImportedMesh> loadImportedMeshes(const std::string& filePath);

    // Load a texture from file
    static bgfx::TextureHandle loadTextureFile(const char* filePath);

    // Load DDS textures 
    static bgfx::TextureHandle loadTextureDDS(const char* _filePath);

    // Convert backslashes to forward slashes in paths
    static std::string ConvertBackslashesToForward(const std::string& path);


private:
    // Compute normals for a mesh when they're not provided
    static void computeNormals(std::vector<PosColorVertex>& vertices,
        const std::vector<uint32_t>& indices);

    // Process a node in the Assimp scene graph
    static void processNode(const aiScene* scene,
        aiNode* node,
        const aiMatrix4x4& parentTransform,
        const std::string& baseDir,
        std::vector<ImportedMesh>& importedMeshes);

    // Helper for loading texture memory
    static const bgfx::Memory* loadMem(const char* _filePath);
};