#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "shader.h"

class IRenderPipeline
{
public:
    virtual ~IRenderPipeline() = default;

    // One-time setup: allocate FBOs, compile shaders.
    // lightCount determines NR_POINT_LIGHTS in the compiled shaders.
    virtual void init(int width, int height, int lightCount) = 0;

    // Called once per frame before drawing geometry.
    // Binds the pipeline's target framebuffer and sets per-frame uniforms.
    // lightColors[i] is the RGB colour for point light i (diffuse + specular).
    virtual void beginGeometryPass(const glm::mat4& view,
                                   const glm::mat4& projection,
                                   const glm::vec3& viewPos,
                                   const std::vector<glm::vec3>& lightPositions,
                                   const std::vector<glm::vec3>& lightColors) = 0;

    // Called once per frame after all geometry Draw() calls complete.
    // For Deferred: runs the lighting pass (screen-space quad).
    // For Forward: no-op (all lighting done in geometry shader).
    virtual void endGeometryPass(const glm::vec3& viewPos,
                                 const std::vector<glm::vec3>& lightPositions,
                                 const std::vector<glm::vec3>& lightColors) = 0;

    // Renders small coloured cubes at light positions for debugging.
    // Colours match the per-light colour passed to beginGeometryPass.
    virtual void renderDebugLights(const glm::mat4& view,
                                   const glm::mat4& projection,
                                   const std::vector<glm::vec3>& lightPositions,
                                   const std::vector<glm::vec3>& lightColors) = 0;

    // Returns a reference to the geometry-drawing shader.
    // Model/Mesh use this to set their model matrix and draw.
    virtual Shader& getGeometryShader() = 0;
};
