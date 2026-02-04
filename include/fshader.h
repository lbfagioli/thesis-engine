#pragma once

const char* fragmentSource = R"FSH(
#version 330 core

in vec4 vColor;
in vec2 TexCoords;

out vec4 FragColor;

uniform float fading;
uniform sampler2D texture1;

void main()
{
	FragColor = texture(texture1, TexCoords) * fading * vColor;
}
)FSH";

const char* cubeFSH = R"FSH(
#version 330 core

out vec4 FragColor;

uniform vec3 ownColor;
uniform vec3 lightColor;

void main()
{
	FragColor = vec4(lightColor * ownColor, 1.0);
}
)FSH";



const char* lightFSH = R"FSH(
#version 330 core

out vec4 FragColor;

void main()
{
	FragColor = vec4(1.0);
}
)FSH";