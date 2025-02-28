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