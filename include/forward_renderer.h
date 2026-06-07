#pragma once

#include "i_render_pipeline.h"
#include "shader.h"
#include "vshader.h"
#include "fshader.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

class ForwardRenderer : public IRenderPipeline
{
public:
    ForwardRenderer() = default;

    ~ForwardRenderer()
    {
        if (m_lightCubeVAO != 0) glDeleteVertexArrays(1, &m_lightCubeVAO);
        if (m_lightCubeVBO != 0) glDeleteBuffers(1, &m_lightCubeVBO);
    }

    ForwardRenderer(const ForwardRenderer&) = delete;
    ForwardRenderer& operator=(const ForwardRenderer&) = delete;

    ForwardRenderer(ForwardRenderer&& other) noexcept
        : m_geometryShader(std::move(other.m_geometryShader))
        , m_lightShader(std::move(other.m_lightShader))
        , m_lightCubeVAO(other.m_lightCubeVAO)
        , m_lightCubeVBO(other.m_lightCubeVBO)
        , m_lightCount(other.m_lightCount)
    {
        other.m_lightCubeVAO = 0;
        other.m_lightCubeVBO = 0;
    }

    ForwardRenderer& operator=(ForwardRenderer&& other) noexcept
    {
        if (this != &other)
        {
            if (m_lightCubeVAO) glDeleteVertexArrays(1, &m_lightCubeVAO);
            if (m_lightCubeVBO) glDeleteBuffers(1, &m_lightCubeVBO);
            m_geometryShader = std::move(other.m_geometryShader);
            m_lightShader = std::move(other.m_lightShader);
            m_lightCubeVAO = other.m_lightCubeVAO;
            m_lightCubeVBO = other.m_lightCubeVBO;
            m_lightCount = other.m_lightCount;
            other.m_lightCubeVAO = 0;
            other.m_lightCubeVBO = 0;
        }
        return *this;
    }

    void init(int width, int height, int lightCount) override
    {
        m_lightCount = lightCount;

        // Recompile geometry shader with the correct NR_POINT_LIGHTS
        std::string cubeSrc = getCubeFSH(lightCount);
        m_geometryShader = Shader(cubeVSH, cubeSrc.c_str());
        m_lightShader = Shader(lightVSH, lightFSH);

        // DIAGNOSTIC: Verify the compiled program ID and light count
        std::cout << "[DIAG ForwardRenderer::init] lightCount=" << lightCount
                  << ", m_geometryShader id=" << m_geometryShader.id()
                  << ", m_lightCount=" << m_lightCount << std::endl;

        setupLightCube();
    }

    void beginGeometryPass(const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& viewPos,
                           const std::vector<glm::vec3>& lightPositions,
                           const std::vector<glm::vec3>& lightColors) override
    {
        // Clear the default framebuffer (matching Deferred's clear behaviour)
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_geometryShader.use();
        m_geometryShader.setMat4("view", view);
        m_geometryShader.setMat4("projection", projection);
        m_geometryShader.setVec3("viewPos", viewPos);

        // Point lights only — unified model matching Deferred screenFSH
        size_t nLights = lightPositions.size();
        if (nLights > static_cast<size_t>(m_lightCount))
            nLights = static_cast<size_t>(m_lightCount);

        size_t nColors = lightColors.size();
        for (size_t i = 0; i < nLights; ++i)
        {
            std::string prefix = "pointLights[" + std::to_string(i) + "]";
            m_geometryShader.setVec3(prefix + ".position",  lightPositions[i]);
            m_geometryShader.setFloat(prefix + ".linear",    0.09f);
            m_geometryShader.setFloat(prefix + ".quadratic", 0.032f);
            // Use per-light colour if available, otherwise fall back to white
            glm::vec3 col = (i < nColors) ? lightColors[i] : glm::vec3(1.0f);
            m_geometryShader.setVec3(prefix + ".color", col);
        }

        // Material
        m_geometryShader.setFloat("material.shininess", 32.0f);
    }

    void endGeometryPass(const glm::vec3& /*viewPos*/,
                         const std::vector<glm::vec3>& /*lightPositions*/,
                         const std::vector<glm::vec3>& /*lightColors*/) override
    {
        // Forward renderer does all lighting in the geometry shader during draw.
    }

    void renderDebugLights(const glm::mat4& view,
                           const glm::mat4& projection,
                           const std::vector<glm::vec3>& lightPositions,
                           const std::vector<glm::vec3>& lightColors) override
    {
        m_lightShader.use();
        m_lightShader.setMat4("view", view);
        m_lightShader.setMat4("projection", projection);

        glBindVertexArray(m_lightCubeVAO);
        for (size_t i = 0; i < lightPositions.size(); i++)
        {
            glm::mat4 lightModel(1.0f);
            lightModel = glm::translate(lightModel, lightPositions[i]);
            lightModel = glm::scale(lightModel, glm::vec3(0.2f));
            m_lightShader.setMat4("model", lightModel);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);
    }

    Shader& getGeometryShader() override
    {
        return m_geometryShader;
    }

private:
    Shader m_geometryShader{ cubeVSH, cubeFSH };
    Shader m_lightShader{ lightVSH, lightFSH };

    unsigned int m_lightCubeVAO = 0;
    unsigned int m_lightCubeVBO = 0;

    int m_lightCount = 4;

    void setupLightCube()
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

        glGenVertexArrays(1, &m_lightCubeVAO);
        glGenBuffers(1, &m_lightCubeVBO);

        glBindVertexArray(m_lightCubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_lightCubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
};
