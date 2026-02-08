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

in vec3 Normal;
in vec3 FragPos;
out vec4 FragColor;

uniform vec3 ownColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
	// ambient
	float ambientStrength = 0.1;
	vec3 ambient = ambientStrength * lightColor;

	// diffuse
	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(lightPos - FragPos);

	float impact = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = impact * lightColor;

	// specular
	float specularStrength = 0.5;
	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(reflectDir, viewDir), 0.0), 32);
	vec3 specular = specularStrength * spec * lightColor;

	vec3 color = (ambient + diffuse + specular) * ownColor;

	FragColor = vec4(color, 1.0);
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