#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <array>

struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;
};

bool ObjLoader::loadObj(const std::string& filepath,
    std::vector<Vertex>& vertices,
    std::vector<uint16_t>& indices) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texCoords;
    std::unordered_map<std::string, uint16_t> uniqueVertices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "v") {
            Vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (type == "vn") {
            Vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (type == "vt") {
            Vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            texCoords.push_back(texCoord);
        }
        else if (type == "f") {
            for (int i = 0; i < 3; ++i) {
                std::string vertexData;
                iss >> vertexData;

                if (uniqueVertices.count(vertexData) == 0) {
                    std::istringstream viss(vertexData);
                    std::string indexStr;
                    int indices[3] = { 0, 0, 0 };
                    int j = 0;
                    while (std::getline(viss, indexStr, '/')) {
                        if (!indexStr.empty()) {
                            indices[j] = std::stoi(indexStr) - 1;
                        }
                        ++j;
                    }

                    Vertex vertex;
                    vertex.x = positions[indices[0]].x;
                    vertex.y = positions[indices[0]].y;
                    vertex.z = positions[indices[0]].z;

                    if (indices[1] < texCoords.size()) {
                        vertex.u = texCoords[indices[1]].x;
                        vertex.v = texCoords[indices[1]].y;
                    }
                    else {
                        vertex.u = 0.0f;
                        vertex.v = 0.0f;
                    }

                    if (indices[2] < normals.size()) {
                        vertex.nx = normals[indices[2]].x;
                        vertex.ny = normals[indices[2]].y;
                        vertex.nz = normals[indices[2]].z;
                    }
                    else {
                        vertex.nx = 0.0f;
                        vertex.ny = 1.0f;
                        vertex.nz = 0.0f;
                    }

                    uniqueVertices[vertexData] = static_cast<uint16_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertexData]);
            }
            std::swap(indices[indices.size() - 2], indices[indices.size() - 1]);
        }
    }

    if (normals.empty()) {
        computeNormals(vertices, indices);
    }

    return true;
}

void ObjLoader::computeNormals(std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices) {
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

        // Normalize the normal
        float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0) {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }

		normal.x *= -1.0f;
		normal.y *= -1.0f;
		normal.z *= -1.0f;

        vertices[i0].nx += normal.x; vertices[i0].ny += normal.y; vertices[i0].nz += normal.z;
        vertices[i1].nx += normal.x; vertices[i1].ny += normal.y; vertices[i1].nz += normal.z;
        vertices[i2].nx += normal.x; vertices[i2].ny += normal.y; vertices[i2].nz += normal.z;
    }

    for (auto& vertex : vertices) {
        float length = std::sqrt(vertex.nx * vertex.nx + vertex.ny * vertex.ny + vertex.nz * vertex.nz);
        if (length > 0) {
            vertex.nx /= length;
            vertex.ny /= length;
            vertex.nz /= length;
        }
    }
}

bgfx::VertexBufferHandle ObjLoader::createVertexBuffer(const std::vector<Vertex>& vertices) {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    return bgfx::createVertexBuffer(
        bgfx::makeRef(vertices.data(), sizeof(Vertex) * vertices.size()),
        layout
    );
}

bgfx::IndexBufferHandle ObjLoader::createIndexBuffer(const std::vector<uint16_t>& indices) {
    return bgfx::createIndexBuffer(
        bgfx::makeRef(indices.data(), sizeof(uint16_t) * indices.size())
    );
}