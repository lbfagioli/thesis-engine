#pragma once

const char* vertexSource = R"VSH(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoords;

uniform float rotationIlusion;
uniform float verticalOffset;
uniform mat4 transform;

out vec4 vColor;
out vec2 TexCoords;

void main()
{
	gl_Position = transform * vec4(aPos.x, aPos.y + verticalOffset, aPos.z, 1.0);
	vColor = vec4((aColor.x + gl_Position.x), (aColor.y + gl_Position.y), (aColor.z + gl_Position.z), 1.0);
	TexCoords = aTexCoords;
}
)VSH";