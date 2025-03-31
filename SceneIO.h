// SceneIO.h
#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include "Instance.h" // Assuming you'll move Instance struct definition here

namespace SceneIO {
    std::string openFileDialog(bool save);

    void deleteInstance(Instance* instance);

    void saveInstance(std::ofstream& file, const Instance* instance,
        const std::vector<TextureOption>& availableTextures, int parentID = -1);

    void SaveImportedObjMap(const std::unordered_map<std::string, std::string>& map,
        const std::string& filePath);

    std::unordered_map<std::string, std::string> LoadImportedObjMap(const std::string& filePath);

    void saveSceneToFile(std::vector<Instance*>& instances,
        const std::vector<TextureOption>& availableTextures,
        const std::unordered_map<std::string, std::string>& importedObjMap);

    void findMaxInstanceId(const Instance* instance, int& maxId);

    std::unordered_map<std::string, std::string> loadSceneFromFile(
        std::vector<Instance*>& instances,
        const std::vector<TextureOption>& availableTextures,
        const std::unordered_map<std::string, std::pair<bgfx::VertexBufferHandle, bgfx::IndexBufferHandle>>& bufferMap);
}