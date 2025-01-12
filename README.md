## OpenGL-playground

Libraries not bundled: glm, glfw

#### Ubuntu

    libglm-dev libglfw3 libglfw3-dev


## Instructions for windows compile (temp)

open powershell, not cmd

make sure CMake and git are installed and added to path

```
cmake --version
git --version
```

make sure visual studio is installed, not visual studio code

in powershell:

```
  cd (mkdir C:/dev)
  git clone https://github.com/microsoft/vcpkg.git
  cd vcpkg
  ./bootstrap-vcpkg.bat

  ./vcpkg install glfw3 --triplet x64-windows
  ./vcpkg install glm --triplet x64-windows

```

finally run `build-win.sh` with git bash
