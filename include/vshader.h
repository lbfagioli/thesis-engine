#pragma once

const char* vertexSource = R"VSH(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform float rotationIlusion;
uniform float verticalOffset;
out vec4 vColor;

void main()
{
	gl_Position = vec4(rotationIlusion * aPos.x, aPos.y + verticalOffset, aPos.z, 1.0);
	vColor = vec4((aColor.x + gl_Position.x), (aColor.y + gl_Position.y), (aColor.z + gl_Position.z), 1.0);
}
)VSH";