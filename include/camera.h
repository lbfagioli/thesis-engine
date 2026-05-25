#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

class Camera
{
public:
    Camera(int screenWidth, int screenHeight)
        : m_width(screenWidth), m_height(screenHeight)
    {
    }

    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(m_position, m_position + m_front, m_up);
    }

    glm::mat4 getProjectionMatrix() const
    {
        return glm::perspective(glm::radians(m_fov),
                                static_cast<float>(m_width) / static_cast<float>(m_height),
                                0.1f, 100.0f);
    }

    glm::vec3 position() const { return m_position; }
    glm::vec3 front() const { return m_front; }

    void setScreenSize(int width, int height)
    {
        m_width = width;
        m_height = height;
    }

    void processKeyboard(GLFWwindow* window, float deltaTime)
    {
        const float cameraSpeed = 2.5f * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            m_position += cameraSpeed * m_front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            m_position -= cameraSpeed * m_front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            m_position -= glm::normalize(glm::cross(m_front, m_up)) * cameraSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            m_position += glm::normalize(glm::cross(m_front, m_up)) * cameraSpeed;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    }

    void processMouseMovement(double xposIn, double yposIn)
    {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (m_firstMouse)
        {
            m_lastX = xpos;
            m_lastY = ypos;
            m_firstMouse = false;
            return;
        }

        float xoffset = xpos - m_lastX;
        float yoffset = m_lastY - ypos;
        m_lastX = xpos;
        m_lastY = ypos;

        const float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        m_yaw += xoffset;
        m_pitch += yoffset;

        if (m_pitch > 89.0f)
            m_pitch = 89.0f;
        if (m_pitch < -89.0f)
            m_pitch = -89.0f;

        updateFrontVector();
    }

    void processMouseScroll(double yoffset)
    {
        m_fov -= static_cast<float>(yoffset);
        if (m_fov < 1.0f)
            m_fov = 1.0f;
        if (m_fov > 45.0f)
            m_fov = 45.0f;
    }

private:
    void updateFrontVector()
    {
        glm::vec3 frontDirection;
        frontDirection.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        frontDirection.y = sin(glm::radians(m_pitch));
        frontDirection.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        m_front = glm::normalize(frontDirection);
    }

    glm::vec3 m_position{ 0.0f, 0.0f, 3.0f };
    glm::vec3 m_front{ 0.0f, 0.0f, -1.0f };
    glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };

    float m_yaw = -90.0f;
    float m_pitch = 0.0f;
    float m_fov = 45.0f;

    float m_lastX = 400.0f;
    float m_lastY = 300.0f;
    bool m_firstMouse = true;

    int m_width;
    int m_height;
};
