#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "shader.h"

class IRenderPipeline
{
public:
    virtual ~IRenderPipeline() = default;

    // One-time setup: allocate FBOs, compile shaders
    virtual void init(int width, int height) = 0;

    // Called once per frame before drawing geometry.
    // Binds the pipeline's target framebuffer and sets per-frame uniforms.
    virtual void beginGeometryPass(const glm::mat4& view,
                                   const glm::mat4& projection,
                                   const glm::vec3& viewPos,
                                   const std::vector<glm::vec3>& lightPositions) = 0;

    // Called once per frame after all geometry Draw() calls complete.
    // For Deferred: runs the lighting pass (screen-space quad).
    // For Forward: no-op or minimal cleanup.
    virtual void endGeometryPass(const glm::vec3& viewPos,
                                 const std::vector<glm::vec3>& lightPositions) = 0;

    // Renders small colored cubes at light positions for debugging.
    virtual void renderDebugLights(const glm::mat4& view,
                                   const glm::mat4& projection,
                                   const std::vector<glm::vec3>& lightPositions) = 0;

    // Returns a reference to the geometry-drawing shader.
    // Model/Mesh use this to set their model matrix and draw.
    virtual Shader& getGeometryShader() = 0;

    // Called each frame to update the camera's front vector for flashlight/spotlight.
    virtual void setCameraFront(const glm::vec3& front) = 0;
};
