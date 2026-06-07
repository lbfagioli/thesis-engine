#pragma once

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>

class Shader
{
public:
    Shader() : m_id(0) {}

    Shader(const char* vertexSource, const char* fragmentSource)
    {
        unsigned int vertexShader = makeShader(GL_VERTEX_SHADER, vertexSource);
        unsigned int fragmentShader = makeShader(GL_FRAGMENT_SHADER, fragmentSource);

        m_id = glCreateProgram();
        glAttachShader(m_id, vertexShader);
        glAttachShader(m_id, fragmentShader);
        glLinkProgram(m_id);
        checkBuildError(m_id, "PROGRAM");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    ~Shader()
    {
        if (m_id != 0)
            glDeleteProgram(m_id);
    }

    // Move constructor
    Shader(Shader&& other) noexcept
        : m_id(other.m_id)
    {
        other.m_id = 0;
    }

    // Move assignment
    Shader& operator=(Shader&& other) noexcept
    {
        if (this != &other)
        {
            if (m_id != 0)
                glDeleteProgram(m_id);
            m_id = other.m_id;
            other.m_id = 0;
        }
        return *this;
    }

    // Delete copy operations (OpenGL resources cannot be shallow-copied)
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    unsigned int id() const { return m_id; }

    void use() const
    {
        glUseProgram(m_id);
    }

    void setInt(const std::string& name, int number) const
    {
        glUniform1i(getLoc(name), number);
    }

    void setFloat(const std::string& name, float number) const
    {
        glUniform1f(getLoc(name), number);
    }

    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(getLoc(name), 1, GL_FALSE, &mat[0][0]);
    }

    void setVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(getLoc(name), x, y, z);
    }

    void setVec3(const std::string& name, const glm::vec3& vec) const
    {
        glUniform3fv(getLoc(name), 1, &vec[0]);
    }

private:
    unsigned int m_id = 0;

    unsigned int makeShader(unsigned int SHADER_TYPE, const char* source)
    {
        unsigned int shader = glCreateShader(SHADER_TYPE);
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);

        std::string type = SHADER_TYPE == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT";
        checkBuildError(shader, type);

        return shader;
    }

    void checkBuildError(unsigned int shader, const std::string& type)
    {
        int success;
        char infoLog[512];

        if (type == "PROGRAM")
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 512, NULL, infoLog);
                std::cout << "failed to link shader program\n" << infoLog << std::endl;
            }
        }
        else
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 512, NULL, infoLog);
                std::cout << "failed to compile shader " << type << "\n" << infoLog << std::endl;
            }
        }
    }

    int getLoc(const std::string& name) const
    {
        return glGetUniformLocation(m_id, name.c_str());
    }
};
