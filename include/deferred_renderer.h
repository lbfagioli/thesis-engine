#pragma once

#include "i_render_pipeline.h"
#include "shader.h"
#include "vshader.h"
#include "fshader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>
#include <iostream>

class DeferredRenderer : public IRenderPipeline
{
public:
    DeferredRenderer() = default;

    ~DeferredRenderer()
    {
        cleanupGL();
    }

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    DeferredRenderer(DeferredRenderer&& other) noexcept
        : m_geometryShader(std::move(other.m_geometryShader))
        , m_screenShader(std::move(other.m_screenShader))
        , m_lightShader(std::move(other.m_lightShader))
        , m_gBuffer(other.m_gBuffer)
        , m_gPosition(other.m_gPosition)
        , m_gNormal(other.m_gNormal)
        , m_gAlbedoSpec(other.m_gAlbedoSpec)
        , m_rboDepth(other.m_rboDepth)
        , m_quadVAO(other.m_quadVAO)
        , m_quadVBO(other.m_quadVBO)
        , m_lightCubeVAO(other.m_lightCubeVAO)
        , m_lightCubeVBO(other.m_lightCubeVBO)
        , m_width(other.m_width)
        , m_height(other.m_height)
        , m_lightCount(other.m_lightCount)
    {
        other.m_gBuffer = 0;
        other.m_gPosition = 0;
        other.m_gNormal = 0;
        other.m_gAlbedoSpec = 0;
        other.m_rboDepth = 0;
        other.m_quadVAO = 0;
        other.m_quadVBO = 0;
        other.m_lightCubeVAO = 0;
        other.m_lightCubeVBO = 0;
    }

    DeferredRenderer& operator=(DeferredRenderer&& other) noexcept
    {
        if (this != &other)
        {
            cleanupGL();
            m_geometryShader = std::move(other.m_geometryShader);
            m_screenShader = std::move(other.m_screenShader);
            m_lightShader = std::move(other.m_lightShader);
            m_gBuffer = other.m_gBuffer;
            m_gPosition = other.m_gPosition;
            m_gNormal = other.m_gNormal;
            m_gAlbedoSpec = other.m_gAlbedoSpec;
            m_rboDepth = other.m_rboDepth;
            m_quadVAO = other.m_quadVAO;
            m_quadVBO = other.m_quadVBO;
            m_lightCubeVAO = other.m_lightCubeVAO;
            m_lightCubeVBO = other.m_lightCubeVBO;
            m_width = other.m_width;
            m_height = other.m_height;
            m_lightCount = other.m_lightCount;
            other.m_gBuffer = 0;
            other.m_gPosition = 0;
            other.m_gNormal = 0;
            other.m_gAlbedoSpec = 0;
            other.m_rboDepth = 0;
            other.m_quadVAO = 0;
            other.m_quadVBO = 0;
            other.m_lightCubeVAO = 0;
            other.m_lightCubeVBO = 0;
        }
        return *this;
    }

    void init(int width, int height, int lightCount) override
    {
        m_width = width;
        m_height = height;
        m_lightCount = lightCount;

        // Compile shaders with dynamic light count
        std::string screenSrc = getScreenFSH(lightCount);
        m_geometryShader = Shader(cubeVSH, geometryFSH);
        m_screenShader = Shader(screenVSH, screenSrc.c_str());
        m_lightShader = Shader(lightVSH, lightFSH);

        // DIAGNOSTIC: Verify the compiled program IDs and light count
        std::cout << "[DIAG DeferredRenderer::init] lightCount=" << lightCount
                  << ", m_screenShader (lighting pass) id=" << m_screenShader.id()
                  << ", m_geometryShader id=" << m_geometryShader.id()
                  << ", m_lightCount=" << m_lightCount << std::endl;

        // Create G-Buffer
        glGenFramebuffers(1, &m_gBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer);

        // Position buffer (RGBA16F)
        glGenTextures(1, &m_gPosition);
        glBindTexture(GL_TEXTURE_2D, m_gPosition);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gPosition, 0);

        // Normal buffer (RGBA16F)
        glGenTextures(1, &m_gNormal);
        glBindTexture(GL_TEXTURE_2D, m_gNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gNormal, 0);

        // Albedo+Specular buffer (RGBA8)
        glGenTextures(1, &m_gAlbedoSpec);
        glBindTexture(GL_TEXTURE_2D, m_gAlbedoSpec);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_gAlbedoSpec, 0);

        // Depth renderbuffer
        glGenRenderbuffers(1, &m_rboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_rboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rboDepth);

        // Set draw buffers
        const unsigned int n = 3;
        unsigned int attachments[n] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(n, attachments);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "DeferredRenderer: framebuffer incomplete!" << std::endl;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Pre-set screen shader texture uniforms
        m_screenShader.use();
        m_screenShader.setInt("gPosition", 0);
        m_screenShader.setInt("gNormal", 1);
        m_screenShader.setInt("gAlbedoSpec", 2);

        // Build screen quad
        setupQuad();

        // Build light debug cube
        setupLightCube();
    }

    void beginGeometryPass(const glm::mat4& view,
                           const glm::mat4& projection,
                           const glm::vec3& viewPos,
                           const std::vector<glm::vec3>& /*lightPositions*/,
                           const std::vector<glm::vec3>& /*lightColors*/) override
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer);

        // Clear all G-buffer attachments to (0,0,0,0) so screenFSH can
        // discard background pixels that have no geometry drawn.
        const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        glClearBufferfv(GL_COLOR, 0, zero);  // gPosition
        glClearBufferfv(GL_COLOR, 1, zero);  // gNormal
        glClearBufferfv(GL_COLOR, 2, zero);  // gAlbedoSpec
        glClear(GL_DEPTH_BUFFER_BIT);

        m_geometryShader.use();
        m_geometryShader.setMat4("view", view);
        m_geometryShader.setMat4("projection", projection);
        m_geometryShader.setVec3("viewPos", viewPos);
    }

    void endGeometryPass(const glm::vec3& viewPos,
                         const std::vector<glm::vec3>& lightPositions,
                         const std::vector<glm::vec3>& lightColors) override
    {
        // Unbind G-Buffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Lighting pass — uses per-light colours matching Forward
        m_screenShader.use();
        m_screenShader.setVec3("viewPos", viewPos);

        size_t nLights = lightPositions.size();
        if (nLights > static_cast<size_t>(m_lightCount))
            nLights = static_cast<size_t>(m_lightCount);

        size_t nColors = lightColors.size();
        for (size_t i = 0; i < nLights; ++i)
        {
            m_screenShader.setVec3("pointLights[" + std::to_string(i) + "].position", lightPositions[i]);
            m_screenShader.setFloat("pointLights[" + std::to_string(i) + "].linear", 0.09f);
            m_screenShader.setFloat("pointLights[" + std::to_string(i) + "].quadratic", 0.032f);
            // Use per-light colour if available, otherwise fall back to white
            glm::vec3 col = (i < nColors) ? lightColors[i] : glm::vec3(1.0f);
            m_screenShader.setVec3("pointLights[" + std::to_string(i) + "].color", col);
        }

        // Bind G-Buffer textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_gAlbedoSpec);

        renderQuad();

        // Blit depth from G-Buffer to default framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glActiveTexture(GL_TEXTURE0);
    }

    void renderDebugLights(const glm::mat4& view,
                           const glm::mat4& projection,
                           const std::vector<glm::vec3>& lightPositions,
                           const std::vector<glm::vec3>& /*lightColors*/) override
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
    Shader m_geometryShader{ cubeVSH, geometryFSH };
    Shader m_screenShader{ screenVSH, screenFSH };
    Shader m_lightShader{ lightVSH, lightFSH };

    unsigned int m_gBuffer = 0;
    unsigned int m_gPosition = 0;
    unsigned int m_gNormal = 0;
    unsigned int m_gAlbedoSpec = 0;
    unsigned int m_rboDepth = 0;

    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;

    unsigned int m_lightCubeVAO = 0;
    unsigned int m_lightCubeVBO = 0;

    int m_width = 800;
    int m_height = 600;
    int m_lightCount = 4;

    void setupQuad()
    {
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &m_quadVAO);
        glGenBuffers(1, &m_quadVBO);
        glBindVertexArray(m_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    void setupLightCube()
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

    void renderQuad()
    {
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    void cleanupGL()
    {
        if (m_gBuffer != 0)      { glDeleteFramebuffers(1, &m_gBuffer); m_gBuffer = 0; }
        if (m_gPosition != 0)    { glDeleteTextures(1, &m_gPosition); m_gPosition = 0; }
        if (m_gNormal != 0)      { glDeleteTextures(1, &m_gNormal); m_gNormal = 0; }
        if (m_gAlbedoSpec != 0)  { glDeleteTextures(1, &m_gAlbedoSpec); m_gAlbedoSpec = 0; }
        if (m_rboDepth != 0)     { glDeleteRenderbuffers(1, &m_rboDepth); m_rboDepth = 0; }
        if (m_quadVAO != 0)      { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
        if (m_quadVBO != 0)      { glDeleteBuffers(1, &m_quadVBO); m_quadVBO = 0; }
        if (m_lightCubeVAO != 0) { glDeleteVertexArrays(1, &m_lightCubeVAO); m_lightCubeVAO = 0; }
        if (m_lightCubeVBO != 0) { glDeleteBuffers(1, &m_lightCubeVBO); m_lightCubeVBO = 0; }
    }
};
