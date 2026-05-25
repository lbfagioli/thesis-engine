#pragma once

#include "i_render_pipeline.h"
#include "shader.h"
#include "vshader.h"
#include "fshader.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
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
            other.m_lightCubeVAO = 0;
            other.m_lightCubeVBO = 0;
        }
        return *this;
    }

    void init(int width, int height) override
    {
        setupLightCube();
    }

    void beginGeometryPass(const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& viewPos,
                           const std::vector<glm::vec3>& lightPositions) override
    {
        m_geometryShader.use();
        m_geometryShader.setMat4("view", view);
        m_geometryShader.setMat4("projection", projection);
        m_geometryShader.setVec3("viewPos", viewPos);

        // Directional light
        m_geometryShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        m_geometryShader.setVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
        m_geometryShader.setVec3("dirLight.diffuse", 0.4f, 0.4f, 0.4f);
        m_geometryShader.setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);

        // Point lights
        for (size_t i = 0; i < lightPositions.size() && i < 4; ++i)
        {
            std::string prefix = "pointLights[" + std::to_string(i) + "]";
            m_geometryShader.setVec3(prefix + ".position",  lightPositions[i]);
            m_geometryShader.setFloat(prefix + ".constant",  1.0f);
            m_geometryShader.setFloat(prefix + ".linear",    0.09f);
            m_geometryShader.setFloat(prefix + ".quadratic", 0.032f);
            m_geometryShader.setVec3(prefix + ".ambient",  0.05f, 0.05f, 0.05f);
            m_geometryShader.setVec3(prefix + ".diffuse",  0.8f, 0.8f, 0.8f);
            m_geometryShader.setVec3(prefix + ".specular", 1.0f, 1.0f, 1.0f);
        }

        // Spot light (flashlight attached to camera)
        m_geometryShader.setVec3("spotLight.position", viewPos);
        m_geometryShader.setVec3("spotLight.direction", cameraFront);
        m_geometryShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
        m_geometryShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        m_geometryShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        m_geometryShader.setFloat("spotLight.constant", 1.0f);
        m_geometryShader.setFloat("spotLight.linear", 0.09f);
        m_geometryShader.setFloat("spotLight.quadratic", 0.032f);
        m_geometryShader.setFloat("spotLight.cutoff", glm::cos(glm::radians(12.5f)));
        m_geometryShader.setFloat("spotLight.outerCutoff", glm::cos(glm::radians(15.0f)));

        // Material
        m_geometryShader.setFloat("material.shininess", 32.0f);
    }

    void endGeometryPass(const glm::vec3& viewPos,
                         const std::vector<glm::vec3>& lightPositions) override
    {
        // Forward renderer does all lighting in the geometry shader during draw.
        // Point lights are set externally via getGeometryShader() before each draw call.
    }

    void renderDebugLights(const glm::mat4& view,
                           const glm::mat4& projection,
                           const std::vector<glm::vec3>& lightPositions) override
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

    void setCameraFront(const glm::vec3& front) override { cameraFront = front; }

private:
    Shader m_geometryShader{ cubeVSH, cubeFSH };
    Shader m_lightShader{ lightVSH, lightFSH };

    unsigned int m_lightCubeVAO = 0;
    unsigned int m_lightCubeVBO = 0;

    glm::vec3 cameraFront{ 0.0f, 0.0f, -1.0f };

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
