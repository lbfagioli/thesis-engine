# Thesis Engine

## Setup process to compile the project

This project has external dependencies that are mandatory for it to properly work. In fact, the external/ directory has placeholders that describe its expected structure.

If you want to compile this project, you have two options.

1. Download the external/ directory with all set and ready.
2. Get every library by yourself, compiling some, downloading others.

---

### Option 1: Get the premade the external/ directory

1. Download the Zip compressed external directory.
    Download for Windows: https://drive.google.com/file/d/1yiF4tfHHAf3rv-Xk5GaMHISEj3ViiS2V/view?usp=drive_link

    Download for Linux: https://drive.google.com/file/d/1jvFOEjqcWyu-eBFFFnb0mPykZ4suNtqX/view?usp=drive_link

2. Decompressed the `.zip` file

3. Replace the `external/` directory of this project with the new extraced `external/`

---

### Option 2: Get the libraries

#### Get GLFW (compilation involved)

1. First, you need to download the source package in https://www.glfw.org/download.html

2. If you are in Linux, some packages are needed to complete native compilation. You can ignore this step if you are in Windows.

    Command for Fedora:

    ```bash
    sudo dnf install wayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
    ```

3. Then, you can use a tool like CMake to generate a Makefile in Linux or a Visual Studio solution in Windows. After that, you are able to compile the project.

4. If compilation was successful, you can copy the contents of the `include/` directory to the `external/glfw/includes/` directory of this project.

5. Finally, copy the `glfw3.lib` file (if in Windows) or the `libglfw3.a` (for Linux) to `external/glfw/libs/`.

#### Get OpenGL development package

This is a mandatory step to use OpenGL for development in Linux, this is, calling its functions.

If you use Visual Studio in Windows, you can ignore this step, as it usually comes with the package by default.

Command for Fedora:

```bash
sudo dnf install mesa-libGL-devel
```

#### Get GLAD

This library is used to call OpenGL functions easier.

1. Go to glad.dav1d.de/

2. Select API -> gl -> Version 3.3

3. Select Profile -> Core

4. Tick Options -> Generate a loader

5. Hit the GENERATE button

6. Decompress `glad.zip`

7. Copy the `glad/include/` contents to the `external/glad/includes/` directory of this project

8. Copy the `glad/src/` contents to the `external/glad/src/directory` of this project

#### Get stb_image

This is a header only library used to load images of many different formats.

1. Go to https://github.com/nothings/stb/blob/master/stb_image.h

2. Download the `stb_image.h` file

3. Place the file in `external/stb_image/includes`

#### Get GLM

This is a library used to do OpenGL tailored math.

1. Go to https://glm.g-truc.net/0.9.8/index.html and look for "downloads"

2. You probably reach a github releases page, so you can download a `glm-[version].zip`

3. Decompress the `.zip` file

4. Copy the `glm/` directory (that is inside the root directory of the decompressed .zip) to the `external/glm/includes/` directory of this project

---

### Compile the project

After getting the `external/` directory of this project filled with the needed libraries (via [Option 1](#option-1-get-the-premade-the-external-directory) or [Option 2](#option-2-get-the-libraries)), you can compile this project.

#### Windows

1. Use CMake to generate a Visual Studio solution

2. Open the `.sln` file

3. In Visual Studio, you can now build and run the project

#### Linux

Follow these steps, while being in the root of this project directory.

1. Use CMake to generate a Makefile (default)

    ```bash
    cd build
    cmake ..
    ```

2. Use make to compile the project

    ```bash
    make
    ```

3. You can now run the project

    ```bash
    ./thesis-engine
    ```