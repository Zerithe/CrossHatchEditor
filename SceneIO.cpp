// SceneIO.cpp
#include "SceneIO.h"
#include "MeshLoader.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <windows.h>

namespace fs = std::filesystem;

namespace SceneIO {
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

    // Recursive deletion for hierarchy.
    void deleteInstance(Instance* instance)
    {
        for (Instance* child : instance->children)
        {
            deleteInstance(child);
        }
        delete instance;
    }

    void saveInstance(std::ofstream& file, const Instance* instance,
        const std::vector<TextureOption>& availableTextures, int parentID) {
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

    void SaveImportedObjMap(const std::unordered_map<std::string, std::string>& map, const std::string& filePath) {
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

    void findMaxInstanceId(const Instance* instance, int& maxId) {  
       // Check this instance's ID  
       maxId = (std::max)(maxId, instance->id);  

       // Recursively check all children  
       for (const Instance* child : instance->children) {  
           findMaxInstanceId(child, maxId);  
       }  
    }

    std::unordered_map<std::string, std::string> LoadImportedObjMap(const std::string& filePath) {
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

    void saveSceneToFile(std::vector<Instance*>& instances,
        const std::vector<TextureOption>& availableTextures,
        const std::unordered_map<std::string, std::string>& importedObjMap) {
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

    std::unordered_map<std::string, std::string> loadSceneFromFile(
        std::vector<Instance*>& instances,
        const std::vector<TextureOption>& availableTextures,
        const std::unordered_map<std::string, std::pair<bgfx::VertexBufferHandle, bgfx::IndexBufferHandle>>& bufferMap) {

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

        std::vector<MeshLoader::ImportedMesh> importedMeshes;
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
                        importedMeshes = MeshLoader::loadImportedMeshes(i->second);
                        importedMeshesName = meshType;
                    }
                    MeshLoader::createMeshBuffers(importedMeshes[meshNumber].meshData, vbh, ibh);
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
        // Replace the existing code with this
        int maxId = 0;
        for (const Instance* inst : instances) {
            findMaxInstanceId(inst, maxId);
        }
        instanceCounter = maxId + 1;
        return importedObjMap;
    }
} // namespace SceneIO