#pragma once

const char* vertexSource = R"VSH(
#version 330 core

layout (location = 0) in vec3 aPos;
// layout (location = 1) in vec3 aColor;
layout (location = 1) in vec2 aTexCoords;

uniform float rotationIlusion;
uniform float verticalOffset;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 vColor;
out vec2 TexCoords;

void main()
{
	gl_Position = projection * view * model * vec4(aPos.x, aPos.y + verticalOffset, aPos.z, 1.0);
	vColor = gl_Position;
	TexCoords = aTexCoords;
}
)VSH";

const char* cubeVSH = R"VSH(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform float verticalOffset;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 Normal;
out vec3 FragPos;
out vec2 TexCoords;

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	// Normal = mat3(transpose(inverse(model))) * aNormal; // this is necessary when doing non-uniform scaling, but better send the normal matrix via uniform
	Normal = aNormal;
	FragPos = vec3(model * vec4(aPos, 1.0));
	TexCoords = aTexCoords;
}
)VSH";



const char* lightVSH = R"VSH(
#version 330 core

layout (location = 0) in vec3 aPos;

uniform float verticalOffset;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = projection * view * model * vec4(aPos.x, aPos.y + verticalOffset, aPos.z, 1.0);
}
)VSH";