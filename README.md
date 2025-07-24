# AnitoCrosshatch: A 3D Editor Tool for Cross Hatch Rendered 3D Environments

<img src="https://lh3.googleusercontent.com/d/1pqBux0JVWd3ZCFdFZ2vBPN1Gue9xLHck" alt="AnitoCrosshatchLogo" width="200">
<br>
<img src="https://lh3.googleusercontent.com/d/1tqxoyqiiqj6jN5S9bheEnmuailcY-oWu" alt="AnitoCrosshatchLogo" width="400">

<b>Email: crosshatchingthesis@gmail.com</b>

<b>Socials: 

https://www.facebook.com/DLSUGAMELab

</b>

## DESCRIPTION
AnitoCrosshatch is a 3D editor tool designed to create and edit 3D environments rendered in a cross-hatch style. It provides a user-friendly interface for artists and developers to manipulate 3D models, textures, and lighting to achieve the desired aesthetic.

Our application can perform real-time rendering with the stylized shaders, and can produce a variety of different crosshatching styles with the available parameters of our crosshatching system.

## FEATURES
- **Cross Hatch Rendering**: Utilizes a custom shader to render 3D models with a cross-hatch style.
- **Model Import/Export**: Supports importing and exporting 3D models in various formats.
- **Scene Creation**: Allows users to create and manage 3D scenes with multiple objects.
- **Texture Management**: Allows users to apply and manage textures on 3D models.
- **Lighting Control**: Provides tools to adjust lighting conditions in the 3D environment.
- **User Interface**: Features an intuitive UI for easy navigation and manipulation of 3D objects.
- **Real-time Editing**: Enables real-time editing of 3D models and environments.
- **Crosshatch Shader**: Implements a custom shader that applies a cross-hatch effect to 3D models, enhancing the visual style.
- **Crosshatch Parameters**: Offers a range of parameters to customize the crosshatch effect, including line thickness, angle, and density.
- **Scene Management**: Allows users to create, save, and load scenes with multiple 3D objects.
- **Real-time Preview**: Provides a real-time preview of the cross-hatch rendering as changes are made.
- **Image Export**: Allows users to export rendered images of the 3D scenes with cross-hatch effects.

## GALLERY
<img src="https://lh3.googleusercontent.com/d/1d5xtsXshLkUXKhrfJdCzLPpBAhXf1Jj0" alt="AnitoCrosshatchGallery1" width="700">
<img src="https://lh3.googleusercontent.com/d/1RUC_zcxj59OC6yxK4LLPzBImvr8U5j4i" alt="AnitoCrosshatchGallery2" width="700">
<img src="https://lh3.googleusercontent.com/d/1jz6zFNuqS7J2FBdShbV-0NoUdYeEUlMa" alt="AnitoCrosshatchGallery3" width="700">
<img src="https://lh3.googleusercontent.com/d/1bV-QmuvlffX11LuVGDYMu3yRtFZt3OOf" alt="AnitoCrosshatchGallery4" width="700">
<img src="https://lh3.googleusercontent.com/d/1f1YmFVKUfXxWtaNeDfiWOUKAZF3JPIi7" alt="AnitoCrosshatchGallery5" width="700">
<img src="https://lh3.googleusercontent.com/d/1ygrKi-orm5MzOszY1C52RS0wwYTWH79c" alt="AnitoCrosshatchGallery6" width="700">

## INSTALLER

Coming soom ;)

## REQUIREMENTS
- Windows 10 and above
- Visual Studio 2022 or later
- CMake 3.10 or later

## SETUP 

### <u>PREREQUISITES</u>

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

### glfw

- inside the root directory input the following commands:
```
git clone https://github.com/glfw/glfw
cd glfw
cmake -S. -Bcmake-build
cmake --build cmake-build
```

### imgui (Deprecated)
<s>
- inside the root directory input the following commands:
```
git clone https://github.com/ocornut/imgui
```
</s>
<b>Current ImGui is not compatible with the project, we have saved the specific version in a drive link temporarily</b>
<b>https://drive.google.com/file/d/1TZdXJ-motQRek31hDl7U2WD4qSkyYMCl/view?usp=sharing</b>

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

- Download FFmpeg from https://github.com/GyanD/codexffmpeg/releases/download/7.1.1/ffmpeg-7.1.1-full_build-shared.7z
- Unzip the file and place it in the root directory of the project.
- rename the folder to <b>"ffmpeg"</b> so that the path is `AnitoCrosshatch/ffmpeg`."

## BUILDING THE PROJECT

- Everything is handled by Cmake, so you just need to run the following commands in the root directory:
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
- This will generate the Visual Studio solution files in the `build` directory and build the project in Release mode.
- You can also open Visual Studio with Cmake support and build the project from there. Make sure that the <b>CmakeLists.txt</b> highlighted in the Solution Explorer is the one in the root directory of the project.




## HOW TO PACKAGE AND CREATE AN INSTALLER

1. download nsis https://nsis.sourceforge.io/Download

2. open cmd in build folder

3. run the following command:
```
cmake --build . --config Release
cmake --install . --prefix ./package
cpack
```
4. this will create files in the package folder and create an installer for the app.

- if error occurs try using this command:
```
"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64
```
