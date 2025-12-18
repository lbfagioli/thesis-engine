#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
	unsigned int ID;

	Shader(const char* vertexSource, const char* fragmentSource)
	{
		unsigned int vertexShader = makeShader(GL_VERTEX_SHADER, vertexSource);
		unsigned int fragmentShader = makeShader(GL_FRAGMENT_SHADER, fragmentSource);

		ID = glCreateProgram();
		glAttachShader(ID, vertexShader);
		glAttachShader(ID, fragmentShader);
		glLinkProgram(ID);
		checkBuildError(ID, "PROGRAM");

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
	}

	~Shader()
	{
		glDeleteProgram(ID);
	}

	void use()
	{
		glUseProgram(ID);
	}

	void setFloat(std::string name, float number)
	{
		int uniformLocation = glGetUniformLocation(ID, name.c_str());
		glUniform1f(uniformLocation, number);
	}

private:
	unsigned int makeShader(unsigned int SHADER_TYPE, const char* source)
	{
		unsigned int shader;

		shader = glCreateShader(SHADER_TYPE);
		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);

		std::string type = SHADER_TYPE == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT";

		checkBuildError(shader, type);

		return shader;
	}

	void checkBuildError(unsigned int shader, std::string type)
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
};

#endif