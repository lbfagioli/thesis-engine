cmake_minimum_required(VERSION 3.10)

project(thesis-engine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

configure_file(configuration/root_directory.h.in configuration/root_directory.h)

find_package(OpenGL REQUIRED)

set(EXT_DIR ${CMAKE_SOURCE_DIR}/external)

add_library(glad STATIC ${EXT_DIR}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${EXT_DIR}/glad/includes)

add_library(stb_image STATIC ${EXT_DIR}/stb_image/src/stb_image.cpp)
target_include_directories(stb_image PUBLIC ${EXT_DIR}/stb_image/includes)

add_executable(${PROJECT_NAME}
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/deferred_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/forward_renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/texture.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_BINARY_DIR}/configuration/
    ${CMAKE_SOURCE_DIR}/include
    ${EXT_DIR}/glfw/includes
    ${EXT_DIR}/glm/includes
    ${EXT_DIR}/assimp/includes
)

# target_link_directories(${PROJECT_NAME} PUBLIC
#     ${EXT_DIR}/glfw/libs
# )

target_link_libraries(${PROJECT_NAME} PUBLIC
    OpenGL::GL
    glad
    stb_image
    ${EXT_DIR}/assimp/libs/assimp-vc143-mtd.lib
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${EXT_DIR}/glfw/libs/glfw3.lib
    )
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${EXT_DIR}/glfw/libs/libglfw3.a
        m
        X11
        pthread
        Xrandr
        Xi
        dl
    )
endif()