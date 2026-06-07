// Force NVIDIA GPU on Optimus laptops so the experiment runs on the dGPU,
// not the integrated Intel GPU.  Without this the driver may default to
// the iGPU and produce misleadingly low performance numbers.
#ifdef _WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <random>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "camera.h"
#include "forward_renderer.h"
#include "deferred_renderer.h"
#include "model.h"
#include "texture.h"
#include "root_directory.h"

// ============================================================
// GPU Timer Query helpers
// ============================================================

// Render one measured frame of a technique.
// The GL_TIME_ELAPSED query is started/stopped but the result is NOT
// retrieved here — that happens in bulk after all measurement frames
// to avoid pipeline stalls.
static void renderFrame(IRenderPipeline& renderer,
                        GLuint query,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        const glm::vec3& viewPos,
                        const std::vector<glm::vec3>& lightPositions,
                        const std::vector<glm::vec3>& lightColors,
                        Model& backpack,
                        unsigned int cubeVAO,
                        const glm::vec3* cubePositions,
                        int cubeCount,
                        unsigned int diffuseTex,
                        unsigned int specularTex,
                        unsigned int floorVAO,
                        int floorIndexCount)
{
    glBeginQuery(GL_TIME_ELAPSED, query);

    // -- Geometry pass --
    renderer.beginGeometryPass(view, projection, viewPos, lightPositions, lightColors);

    // Draw floor plane
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -0.5f, 0.0f));
    model = glm::scale(model, glm::vec3(10.0f, 10.0f, 10.0f));
    renderer.getGeometryShader().setMat4("model", model);
    glBindVertexArray(floorVAO);
    glDrawElements(GL_TRIANGLES, floorIndexCount, GL_UNSIGNED_INT, 0);

    // Draw backpack
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
    renderer.getGeometryShader().setMat4("model", model);
    backpack.Draw(renderer.getGeometryShader());

    // Draw cubes with textures
    useTexture(diffuseTex, 0);
    useTexture(specularTex, 1);
    glBindVertexArray(cubeVAO);
    for (int i = 0; i < cubeCount; i++)
    {
        model = glm::mat4(1.0f);
        model = glm::translate(model, cubePositions[i]);
        float angle = 20.0f * static_cast<float>(i);
        model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
        renderer.getGeometryShader().setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);

    // -- Lighting / composite pass --
    renderer.endGeometryPass(viewPos, lightPositions, lightColors);

    // Debug light cubes (render even during measurement to match real pipeline)
    renderer.renderDebugLights(view, projection, lightPositions, lightColors);

    glEndQuery(GL_TIME_ELAPSED);
}

// ============================================================
// Statistics
// ============================================================
struct Stats {
    double mean;
    double stddev;
    double min;
    double max;
};

static Stats computeStats(const std::vector<double>& values)
{
    Stats s{};
    if (values.empty()) return s;

    double sum = 0.0;
    s.min = values[0];
    s.max = values[0];
    for (double v : values) {
        sum += v;
        if (v < s.min) s.min = v;
        if (v > s.max) s.max = v;
    }
    s.mean = sum / static_cast<double>(values.size());

    double sqSum = 0.0;
    for (double v : values) {
        double diff = v - s.mean;
        sqSum += diff * diff;
    }
    s.stddev = std::sqrt(sqSum / static_cast<double>(values.size() - 1));
    return s;
}

// ============================================================
// Floor plane geometry
// ============================================================
static void setupFloor(unsigned int* vao, unsigned int* vbo, unsigned int* ebo, int* indexCount)
{
    // A simple textured quad on the XZ plane at y=0, normal pointing up
    float vertices[] = {
        // positions            // normals           // texcoords
        -5.0f, 0.0f, -5.0f,     0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         5.0f, 0.0f, -5.0f,     0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         5.0f, 0.0f,  5.0f,     0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        -5.0f, 0.0f,  5.0f,     0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
    };
    unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };
    *indexCount = 6;

    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
    glGenBuffers(1, ebo);

    glBindVertexArray(*vao);

    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

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
// Cube geometry (same as main.cpp)
// ============================================================
static void setupCube(unsigned int* vao, unsigned int* vbo)
{
    float vertices[] = {
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

    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
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
// CSV output helper
// ============================================================
static void writePerFrameCSV(const std::string& filename,
                              int lightCount, int repetition,
                              const std::string& technique,
                              const std::vector<double>& gpuTimes,
                              const std::vector<double>& totalTimes)
{
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open " << filename << std::endl;
        return;
    }
    for (size_t frame = 0; frame < gpuTimes.size(); frame++) {
        file << lightCount << ","
             << repetition << ","
             << technique << ","
             << frame << ","
             << gpuTimes[frame] << ","
             << totalTimes[frame] << "\n";
    }
}

static void writeSummaryCSV(const std::string& filename,
                             int lightCount,
                             const std::string& technique,
                             const Stats& gpuStats,
                             const Stats& totalStats)
{
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open " << filename << std::endl;
        return;
    }
    double gpuFps = (gpuStats.mean > 0.0) ? (1000.0 / gpuStats.mean) : 0.0;
    double totalFps = (totalStats.mean > 0.0) ? (1000.0 / totalStats.mean) : 0.0;
    file << lightCount << ","
         << technique << ","
         << gpuFps << ","
         << gpuStats.mean << ","
         << gpuStats.stddev << ","
         << gpuStats.min << ","
         << gpuStats.max << ","
         << totalFps << ","
         << totalStats.mean << ","
         << totalStats.stddev << ","
         << totalStats.min << ","
         << totalStats.max << "\n";
}

// ============================================================
// Main experiment
// ============================================================
int main()
{
    // ---- GLFW + GLAD init (hidden window for headless-like measure) ----
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // Window is visible so the GPU actually executes rendering commands
    // (hidden windows may cause drivers to skip GPU work, invalidating GL_TIME_ELAPSED)

    const unsigned int SCR_WIDTH = 800;
    const unsigned int SCR_HEIGHT = 600;
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "thesis-experiment", NULL, NULL);
    if (!window) {
        std::cerr << "ERROR: Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable VSync — critical for valid measurements

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "ERROR: Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // ---- Load assets ----
    unsigned int diffuseTexture  = getTexture("assets/container2.png");
    unsigned int specularTexture = getTexture("assets/container2_specular.png");

    Model backpack(std::string(root_directory) + "/assets/backpack/backpack.obj");

    // ---- Setup geometry ----
    unsigned int cubeVAO = 0, cubeVBO = 0;
    setupCube(&cubeVAO, &cubeVBO);

    unsigned int floorVAO = 0, floorVBO = 0, floorEBO = 0;
    int floorIndexCount = 0;
    setupFloor(&floorVAO, &floorVBO, &floorEBO, &floorIndexCount);

    // ---- Scattered cube positions (10 cubes, semi-random but deterministic) ----
    glm::vec3 cubePositions[10] = {
        glm::vec3( 2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3( 2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f,  3.0f, -7.5f),
        glm::vec3( 1.3f, -2.0f, -2.5f),
        glm::vec3( 1.5f,  2.0f, -2.5f),
        glm::vec3( 1.5f,  0.2f, -1.5f),
        glm::vec3(-1.3f,  1.0f, -1.5f),
        glm::vec3( 0.0f,  0.5f, -5.0f),
    };

    // ---- Fixed camera at (0, 2, 5) looking at origin ----
    Camera camera(SCR_WIDTH, SCR_HEIGHT);
    // Camera defaults: position (0,0,3) front (0,0,-1). We need position (0,2,5).
    // Override via a simple hack: set a custom view matrix
    glm::vec3 camPos(0.0f, 2.0f, 5.0f);
    glm::vec3 camFront = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - camPos);

    glm::mat4 view = glm::lookAt(camPos, camPos + camFront, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = camera.getProjectionMatrix();

    // ---- Timestamp for unique output filenames (YYYY-MM-DD_HH-MM-SS) ----
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S");
    std::string timestamp = ts.str();

    // ---- Random light positions and colors (seeded for reproducibility) ----
    const int MAX_LIGHTS = 64;
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<float> posDist(-4.0f, 4.0f);
    std::uniform_real_distribution<float> colorDist(0.3f, 1.0f);

    std::vector<glm::vec3> allLightPositions(MAX_LIGHTS);
    std::vector<glm::vec3> allLightColors(MAX_LIGHTS);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        allLightPositions[i] = glm::vec3(posDist(rng), posDist(rng), posDist(rng));
        allLightColors[i] = glm::vec3(colorDist(rng), colorDist(rng), colorDist(rng));
    }

    // ---- GPU timer query objects (one per frame to avoid pipeline stalls) ----
    const int MAX_QUERIES = 300;  // must be >= measureFrames
    std::vector<GLuint> forwardQueries(MAX_QUERIES);
    std::vector<GLuint> deferredQueries(MAX_QUERIES);
    glGenQueries(MAX_QUERIES, forwardQueries.data());
    glGenQueries(MAX_QUERIES, deferredQueries.data());

    // ---- CSV filenames with timestamp ----
    std::string perFrameFile = "experiment_perframe_" + timestamp + ".csv";
    std::string summaryFile  = "experiment_summary_"  + timestamp + ".csv";

    // ---- CSV headers ----
    {
        std::ofstream pf(perFrameFile);
        pf << "lightCount,repetition,technique,frame,gpuTimeMs,totalTimeMs\n";
        std::ofstream sf(summaryFile);
        sf << "lightCount,technique,"
           << "gpuFps,meanGpuTimeMs,stddevGpuTimeMs,minGpuTimeMs,maxGpuTimeMs,"
           << "totalFps,meanTotalTimeMs,stddevTotalTimeMs,minTotalTimeMs,maxTotalTimeMs\n";
    }

    // ---- Experiment configurations ----
    const int lightConfigs[] = { 2, 4, 8, 16, 24, 32, 48, 64 };
    const int numLightConfigs = sizeof(lightConfigs) / sizeof(lightConfigs[0]);
    const int numRepetitions = 5;
    const int warmupFrames = 60;
    const int measureFrames = 300;

    std::cout << "===== THESIS EXPERIMENT RUNNER =====" << std::endl;
    std::cout << "Configs: 2, 4, 8, 16, 24, 32, 48, 64 lights x " << numRepetitions << " reps" << std::endl;
    std::cout << "Warmup: " << warmupFrames << " frames | Measure: " << measureFrames << " frames" << std::endl;
    std::cout << "Output: " << perFrameFile << ", " << summaryFile << std::endl;
    std::cout << "====================================" << std::endl;

    // ---- Experiment loop ----
    for (int lightCount : lightConfigs)
    {
        // Select the first `lightCount` lights from the pre-generated pool
        std::vector<glm::vec3> lightPositions(allLightPositions.begin(), allLightPositions.begin() + lightCount);
        std::vector<glm::vec3> lightColors(allLightColors.begin(), allLightColors.begin() + lightCount);

        std::cout << "\n--- Light count: " << lightCount << " ---" << std::endl;

        for (int rep = 0; rep < numRepetitions; rep++)
        {
            bool forwardFirst = (rep % 2 == 0); // Even reps: forward first

            std::cout << "  Rep " << (rep + 1) << "/" << numRepetitions
                      << " (" << (forwardFirst ? "F→D" : "D→F") << ") ... ";

            // ---- Create fresh renderers for this light config ----
            auto forwardRenderer = std::make_unique<ForwardRenderer>();
            auto deferredRenderer = std::make_unique<DeferredRenderer>();
            forwardRenderer->init(SCR_WIDTH, SCR_HEIGHT, lightCount);
            deferredRenderer->init(SCR_WIDTH, SCR_HEIGHT, lightCount);

            // ---- WARMUP (both techniques, no measurement) ----
            for (int w = 0; w < warmupFrames; w++)
            {
                // Forward warmup render (no query)
                forwardRenderer->beginGeometryPass(view, projection, camPos, lightPositions, lightColors);
                {
                    glm::mat4 m(1.0f);
                    m = glm::translate(m, glm::vec3(0.0f, -0.5f, 0.0f));
                    m = glm::scale(m, glm::vec3(10.0f));
                    forwardRenderer->getGeometryShader().setMat4("model", m);
                    glBindVertexArray(floorVAO);
                    glDrawElements(GL_TRIANGLES, floorIndexCount, GL_UNSIGNED_INT, 0);

                    m = glm::mat4(1.0f);
                    forwardRenderer->getGeometryShader().setMat4("model", m);
                    backpack.Draw(forwardRenderer->getGeometryShader());

                    useTexture(diffuseTexture, 0);
                    useTexture(specularTexture, 1);
                    glBindVertexArray(cubeVAO);
                    for (int i = 0; i < 10; i++) {
                        m = glm::mat4(1.0f);
                        m = glm::translate(m, cubePositions[i]);
                        m = glm::rotate(m, glm::radians(20.0f * static_cast<float>(i)), glm::vec3(1.0f, 0.3f, 0.5f));
                        forwardRenderer->getGeometryShader().setMat4("model", m);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                    glBindVertexArray(0);
                }
                forwardRenderer->endGeometryPass(camPos, lightPositions, lightColors);
                forwardRenderer->renderDebugLights(view, projection, lightPositions, lightColors);
                glfwSwapBuffers(window);

                // Deferred warmup render (no query)
                deferredRenderer->beginGeometryPass(view, projection, camPos, lightPositions, lightColors);
                {
                    glm::mat4 m(1.0f);
                    m = glm::translate(m, glm::vec3(0.0f, -0.5f, 0.0f));
                    m = glm::scale(m, glm::vec3(10.0f));
                    deferredRenderer->getGeometryShader().setMat4("model", m);
                    glBindVertexArray(floorVAO);
                    glDrawElements(GL_TRIANGLES, floorIndexCount, GL_UNSIGNED_INT, 0);

                    m = glm::mat4(1.0f);
                    deferredRenderer->getGeometryShader().setMat4("model", m);
                    backpack.Draw(deferredRenderer->getGeometryShader());

                    useTexture(diffuseTexture, 0);
                    useTexture(specularTexture, 1);
                    glBindVertexArray(cubeVAO);
                    for (int i = 0; i < 10; i++) {
                        m = glm::mat4(1.0f);
                        m = glm::translate(m, cubePositions[i]);
                        m = glm::rotate(m, glm::radians(20.0f * static_cast<float>(i)), glm::vec3(1.0f, 0.3f, 0.5f));
                        deferredRenderer->getGeometryShader().setMat4("model", m);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                    glBindVertexArray(0);
                }
                deferredRenderer->endGeometryPass(camPos, lightPositions, lightColors);
                deferredRenderer->renderDebugLights(view, projection, lightPositions, lightColors);
                glfwSwapBuffers(window);
            }

            // ---- MEASUREMENT ----
            // Use one query object per measured frame so we can retrieve all
            // GPU times in a single batch at the end, avoiding per-frame
            // pipeline stalls that would distort the wall-clock measurement.
            std::vector<double> forwardGpuTimes, forwardTotalTimes;
            std::vector<double> deferredGpuTimes, deferredTotalTimes;
            forwardGpuTimes.reserve(measureFrames);
            forwardTotalTimes.reserve(measureFrames);
            deferredGpuTimes.reserve(measureFrames);
            deferredTotalTimes.reserve(measureFrames);

            auto measureTechnique = [&](bool isForward,
                                        std::vector<double>& gpuTimes,
                                        std::vector<double>& totalTimes,
                                        std::vector<GLuint>& queries) {
                IRenderPipeline& renderer = isForward
                    ? static_cast<IRenderPipeline&>(*forwardRenderer)
                    : static_cast<IRenderPipeline&>(*deferredRenderer);

                std::cout << "[DIAG measureTechnique] lightCount=" << lightCount
                          << ", technique=" << (isForward ? "FORWARD" : "DEFERRED")
                          << ", measureFrames=" << measureFrames << std::endl;

                // Render all measurement frames — do NOT retrieve GPU times yet
                for (int f = 0; f < measureFrames; f++)
                {
                    auto frameStart = std::chrono::high_resolution_clock::now();

                    renderFrame(renderer, queries[f],
                                view, projection, camPos, lightPositions, lightColors,
                                backpack, cubeVAO, cubePositions, 10,
                                diffuseTexture, specularTexture,
                                floorVAO, floorIndexCount);

                    glfwSwapBuffers(window);

                    auto frameEnd = std::chrono::high_resolution_clock::now();
                    double totalMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
                    totalTimes.push_back(totalMs);
                }

                // Now retrieve all GPU times in bulk (GPU has already finished
                // these frames, so no pipeline stall).
                for (int f = 0; f < measureFrames; f++)
                {
                    GLuint64 gpuTimeNs = 0;
                    glGetQueryObjectui64v(queries[f], GL_QUERY_RESULT, &gpuTimeNs);
                    double gpuMs = static_cast<double>(gpuTimeNs) / 1e6; // ns -> ms
                    gpuTimes.push_back(gpuMs);
                }
            };

            if (forwardFirst) {
                measureTechnique(true,  forwardGpuTimes, forwardTotalTimes, forwardQueries);
                measureTechnique(false, deferredGpuTimes, deferredTotalTimes, deferredQueries);
            } else {
                measureTechnique(false, deferredGpuTimes, deferredTotalTimes, deferredQueries);
                measureTechnique(true,  forwardGpuTimes, forwardTotalTimes, forwardQueries);
            }

            // ---- Compute statistics ----
            Stats fwdGpuStats = computeStats(forwardGpuTimes);
            Stats defGpuStats = computeStats(deferredGpuTimes);
            Stats fwdTotalStats = computeStats(forwardTotalTimes);
            Stats defTotalStats = computeStats(deferredTotalTimes);

            // ---- Write CSV ----
            writePerFrameCSV(perFrameFile, lightCount, rep, "forward",
                             forwardGpuTimes, forwardTotalTimes);
            writePerFrameCSV(perFrameFile, lightCount, rep, "deferred",
                             deferredGpuTimes, deferredTotalTimes);

            writeSummaryCSV(summaryFile, lightCount, "forward_" + std::to_string(rep),
                            fwdGpuStats, fwdTotalStats);
            writeSummaryCSV(summaryFile, lightCount, "deferred_" + std::to_string(rep),
                            defGpuStats, defTotalStats);

            // ---- Console output ----
            double fwdGpuFps = 1000.0 / fwdGpuStats.mean;
            double defGpuFps = 1000.0 / defGpuStats.mean;
            double fwdTotalFps = 1000.0 / fwdTotalStats.mean;
            double defTotalFps = 1000.0 / defTotalStats.mean;
            double fwdOverhead = fwdTotalStats.mean - fwdGpuStats.mean;
            double defOverhead = defTotalStats.mean - defGpuStats.mean;

            std::cout << "\n  Forward:  GPU " << fwdGpuStats.mean << " ms ("
                      << fwdGpuFps << " FPS)"
                      << " | Total " << fwdTotalStats.mean << " ms ("
                      << fwdTotalFps << " FPS)"
                      << " | CPU overhead " << fwdOverhead << " ms" << std::endl;
            std::cout << "  Deferred: GPU " << defGpuStats.mean << " ms ("
                      << defGpuFps << " FPS)"
                      << " | Total " << defTotalStats.mean << " ms ("
                      << defTotalFps << " FPS)"
                      << " | CPU overhead " << defOverhead << " ms" << std::endl;

            // ---- Cooldown between reps ----
            std::cout << "    Cooling down 5s..." << std::flush;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << " done." << std::endl;
        }

        // ---- Cooldown between light configs (thermal/power state reset) ----
        std::cout << "  Cooling down 2s before next light config..." << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << " done." << std::endl;
    }

    // ---- Cleanup ----
    glDeleteQueries(MAX_QUERIES, forwardQueries.data());
    glDeleteQueries(MAX_QUERIES, deferredQueries.data());
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &floorEBO);

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\n===== EXPERIMENT COMPLETE =====" << std::endl;
    return 0;
}
