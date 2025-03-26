#include "MeshLoader.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cfloat>

// For texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

void MeshLoader::computeNormals(std::vector<PosColorVertex>& vertices, const std::vector<uint32_t>& indices) {
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

MeshLoader::MeshData MeshLoader::loadMesh(const std::string& filePath) {
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

void MeshLoader::createMeshBuffers(const MeshData& meshData, bgfx::VertexBufferHandle& vbh, bgfx::IndexBufferHandle& ibh) {
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

const bgfx::Memory* MeshLoader::loadMem(const char* _filePath)
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

bgfx::TextureHandle MeshLoader::loadTextureDDS(const char* _filePath)
{
    const bgfx::Memory* mem = loadMem(_filePath);
    if (mem == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }
    // Create texture from memory.
    return bgfx::createTexture(mem);
}

std::string MeshLoader::ConvertBackslashesToForward(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

bgfx::TextureHandle MeshLoader::loadTextureFile(const char* filePath)
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

        // Free stb_image's data
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

void MeshLoader::processNode(const aiScene* scene, aiNode* node, const aiMatrix4x4& parentTransform,
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

        // Create an ImportedMesh to store this mesh's data.
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

std::vector<MeshLoader::ImportedMesh> MeshLoader::loadImportedMeshes(const std::string& filePath) {
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