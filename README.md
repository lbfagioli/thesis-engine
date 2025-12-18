# Thesis Engine

## Linux environment setup

### Compiling GLFW

In linux, some packages are needed to complete native compilation. Command for fedora:

sudo dnf install wayland-devel libxkbcommon-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel

### Getting OpenGL development package

Needed to use OpenGL for development, this is, calling its functions.

sudo dnf install mesa-libGL-devel