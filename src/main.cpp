#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>

#include "camera.h"
#include "i_render_pipeline.h"
#include "deferred_renderer.h"
#include "forward_renderer.h"
#include "model.h"
#include "texture.h"
#include "root_directory.h"

// ---- Window ----
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// ---- Physics ----
const float GRAVITY = -9.8f;
const float JUMP_POWER = 8.0f;

float deltaTime = 0.0f;

glm::vec3 cubePos(0.0f, 0.0f, 0.0f);

// ---- Callbacks (forwarded to Camera) ----
Camera* g_camera = nullptr;

void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
    if (g_camera) g_camera->setScreenSize(width, height);
}

void mouse_callback(GLFWwindow* /*window*/, double xpos, double ypos)
{
    if (g_camera) g_camera->processMouseMovement(xpos, ypos);
}

void scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
{
    if (g_camera) g_camera->processMouseScroll(yoffset);
}

// ---- Cube Geometry (manual VAO, not Assimp) ----
unsigned int cubeVAO = 0, cubeVBO = 0;

void setupCube()
{
    float vertices[] = {
        // positions          // normals           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ============================================================
int main()
{
    // ---- GLFW + GLAD init ----
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "thesis-engine", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    // glfwSwapInterval(0); // Explicitly disable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    int displayWidth, displayHeight;
    glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
    glViewport(0, 0, displayWidth, displayHeight);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glEnable(GL_DEPTH_TEST);

    // ---- Camera ----
    Camera camera(displayWidth, displayHeight);
    g_camera = &camera;

    // ---- Renderer (F1 to toggle Forward ↔ Deferred) ----
    std::unique_ptr<IRenderPipeline> renderer = std::make_unique<DeferredRenderer>();
    renderer->init(displayWidth, displayHeight);

    bool f1Held = false;

    // ---- Textures ----
    unsigned int diffuseTexture  = getTexture("assets/container2.png");
    unsigned int specularTexture = getTexture("assets/container2_specular.png");

    // ---- Models & Geometry ----
    Model backpack(std::string(root_directory) + "/assets/backpack/backpack.obj");
    setupCube();

    // ---- Light positions ----
    std::vector<glm::vec3> lightPositions = {
        glm::vec3( 0.7f,  0.2f,  2.0f),
        glm::vec3( 2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f,  2.0f, -12.0f),
        glm::vec3( 0.0f,  0.0f, -3.0f)
    };

    glm::vec3 cubePositions[] = {
        glm::vec3( 2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3( 2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f,  3.0f, -7.5f),
        glm::vec3( 1.3f, -2.0f, -2.5f),
        glm::vec3( 1.5f,  2.0f, -2.5f),
        glm::vec3( 1.5f,  0.2f, -1.5f),
        glm::vec3(-1.3f,  1.0f, -1.5f)
    };

    // ---- Physics state ----
    float velocityY = 0.0f;
    float lastTime   = (float)glfwGetTime();
    float lastSeconds = 0.0f;
    int   frames = 0;

    // ---- Render loop ----
    while (!glfwWindowShouldClose(window))
    {
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // FPS counter
        int seconds = (int)currentTime;
        if (seconds != (int)lastSeconds)
        {
            std::cout << "FPS: " << frames / (seconds - (int)lastSeconds) << std::endl;
            frames = 0;
            lastSeconds = (float)seconds;
        }
        frames++;

        // Input
        camera.processKeyboard(window, deltaTime);

        // F1 toggle: swap render pipelines at runtime
        {
            bool f1Pressed = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1Pressed && !f1Held)
            {
                // Determine which type we're currently using by trying a dynamic_cast
                bool isDeferred = (dynamic_cast<DeferredRenderer*>(renderer.get()) != nullptr);
                renderer.reset();
                if (isDeferred)
                    renderer = std::make_unique<ForwardRenderer>();
                else
                    renderer = std::make_unique<DeferredRenderer>();
                renderer->init(displayWidth, displayHeight);
                std::cout << "[TOGGLE] Switched to "
                          << (isDeferred ? "Forward" : "Deferred")
                          << " Renderer" << std::endl;
            }
            f1Held = f1Pressed;
        }

        // Physics
        velocityY += (/*acceleration=*/0.0f + GRAVITY) * deltaTime;
        cubePos.y += (velocityY * deltaTime) / 5.0f;
        if (cubePos.y < -0.5f) { cubePos.y = -0.5f; velocityY = 0.0f; }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) velocityY = JUMP_POWER;

        // Matrices
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix();

        // ---- Geometry Pass ----
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderer->setCameraFront(camera.front());
        renderer->beginGeometryPass(view, projection, camera.position(), lightPositions);

        // Draw backpack model
        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        renderer->getGeometryShader().setMat4("model", model);
        backpack.Draw(renderer->getGeometryShader());

        // Draw 9 cubes with textures
        useTexture(diffuseTexture, 0);
        useTexture(specularTexture, 1);
        glBindVertexArray(cubeVAO);

        for (unsigned int i = 0; i < 9; i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, cubePos);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * (float)i;
            model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            renderer->getGeometryShader().setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);

        // ---- Lighting / Composite Pass ----
        renderer->endGeometryPass(camera.position(), lightPositions);

        // ---- Debug Light Cubes ----
        renderer->renderDebugLights(view, projection, lightPositions);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);

    glfwTerminate();
    return 0;
}
