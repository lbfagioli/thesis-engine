cmake_minimum_required(VERSION 3.10)

project(thesis-engine LANGUAGES CXX C)

add_executable(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/external/glad/src/glad.c
)

configure_file(configuration/root_directory.h.in configuration/root_directory.h)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_BINARY_DIR}/configuration/
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/glad/includes
    ${CMAKE_SOURCE_DIR}/external/glfw/includes
    ${CMAKE_SOURCE_DIR}/external/glm/includes
    ${CMAKE_SOURCE_DIR}/external/stb_image/includes
)

target_link_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/external/glfw/libs
)

find_package(OpenGL REQUIRED)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        glfw3.lib
        OpenGL::GL
    )
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC
        libglfw3.a
        OpenGL::GL
        m
        X11
        pthread
        Xrandr
        Xi
        dl
    )
endif()