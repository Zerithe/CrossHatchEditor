# CrossHatchEditor
 
## DESCRIPTION

## SETUP 

### bgfx.cmake

- inside the root directory input the following commands:
```
git clone https://github.com/bkaradzic/bgfx.cmake.git
cd bgfx.cmake
git submodule init
git submodule update
cmake -S. -Bcmake-build
cmake --build cmake-build
```
*if setup fails send to messenger or discord to check*

### glfw

- inside the root directory input the following commands:
```
git clone https://github.com/glfw/glfw
cd glfw
cmake -S. -Bcmake-build
cmake --build cmake-build
```

### imgui

- inside the root directory input the following commands:
```
git clone https://github.com/ocornut/imgui
```

### assimp

- inside the root directory input the following commands:
```
git clone https://github.com/assimp/assimp.git
cd assimp
cmake CMakeLists.txt
cmake --build .
```

### imguizmo

- inside the root directory input the following commands:
```
git clone https://github.com/CedricGuillemet/ImGuizmo
```

### FFmpeg

- Download FFmpeg from https://github.com/GyanD/codexffmpeg/releases/download/7.1.1/ffmpeg-7.1.1-full_build.zip
- Unzip the file and place it in the root directory of the project.

### HOW TO PACKAGE AND BUILD

1. open cmd in build folder

2. run the following command:
```
cmake --build . --config Release
cmake --install . --prefix ./package
cpack
```
3. this will create files in the package folder and create an installer for the app.

- if error occurs try using this command:
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
```
