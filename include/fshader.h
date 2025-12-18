#pragma once

const char* fragmentSource = R"FSH(
#version 330 core

in vec4 vColor;
out vec4 FragColor;
uniform float fading;

void main()
{
	FragColor = fading * vColor;
}
)FSH";