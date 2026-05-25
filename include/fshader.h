#pragma once

inline const char* fragmentSource = R"FSH(
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

inline const char* cubeFSH = R"FSH(
#version 330 core

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

out vec4 FragColor;

struct Material {
	// vec3 ambient;
	sampler2D texture_diffuse1;
	sampler2D texture_specular1;
	float shininess;
};

struct DirLight {
	vec3 direction;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight {
	vec3 position;

	float constant;
	float linear;
	float quadratic;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct SpotLight {
	vec3 position;
	vec3 direction;
	float cutoff;
	float outerCutoff;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;

	float constant;
	float linear;
	float quadratic;
};

#define NR_POINT_LIGHTS 4

uniform Material material;
uniform vec3 viewPos;

uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{
	vec3 norm = normalize(Normal);
	vec3 viewDir = normalize(viewPos - FragPos);

	vec3 color = CalcDirLight(dirLight, norm, viewDir);

	for (int i = 0; i < NR_POINT_LIGHTS; i++)
		color += CalcPointLight(pointLights[i], norm, FragPos, viewDir);
	
	// color += CalcSpotLight(spotLight, norm, FragPos, viewDir);

	FragColor = vec4(color, 1.0);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
	vec3 lightDir = normalize(-light.direction);

	float diff = max(dot(normal, lightDir), 0.0);

	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

	vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse1, TexCoords));
	vec3 diffuse = light.diffuse * diff * vec3(texture(material.texture_diffuse1, TexCoords));
	vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords));

	return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
	// ambient
	vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse1, TexCoords));

	// diffuse
	vec3 lightDir = normalize(light.position - fragPos);

	float impact = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.diffuse * impact * vec3(texture(material.texture_diffuse1, TexCoords));

	// specular
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords));

	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

	ambient *= attenuation;
	diffuse *= attenuation;
	specular *= attenuation;

	return (ambient + diffuse + specular);
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
	vec3 lightDir = normalize(light.position - fragPos);
	float theta = dot(lightDir, normalize(-light.direction));
	float epsilon = light.cutoff - light.outerCutoff;
	float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0, 1.0);
	
	// ambient
	vec3 ambient = light.ambient * vec3(texture(material.texture_diffuse1, TexCoords));

	// diffuse
	float impact = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.diffuse * impact * vec3(texture(material.texture_diffuse1, TexCoords));

	// specular
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	vec3 specular = light.specular * spec * vec3(texture(material.texture_specular1, TexCoords));

	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

	ambient *= attenuation;
	diffuse *= attenuation * intensity;
	specular *= attenuation * intensity;

	return (ambient + diffuse + specular);
}
)FSH";



inline const char* lightFSH = R"FSH(
#version 330 core

out vec4 FragColor;

void main()
{
	FragColor = vec4(1.0);
}
)FSH";


inline const char* geometryFSH = R"FSH(
#version 330 core

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

struct Material {
	sampler2D texture_diffuse1;
	sampler2D texture_specular1;
	float shininess;
};

uniform Material material;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

void main()
{
	gPosition = FragPos;
	gNormal = normalize(Normal);
	gAlbedoSpec.rgb = texture(material.texture_diffuse1, TexCoords).rgb;
	gAlbedoSpec.a = texture(material.texture_specular1, TexCoords).r;
}
)FSH";

inline const char* screenFSH = R"FSH(
#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

struct PointLight {
	vec3 position;

	float linear;
	float quadratic;

	vec3 color;
};

#define NR_POINT_LIGHTS 4

uniform vec3 viewPos;
uniform PointLight pointLights[NR_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 diffuse_arg, float specular_arg, vec3 viewDir);

void main()
{
	vec3 FragPos = texture(gPosition, TexCoords).rgb;
	vec3 Normal = texture(gNormal, TexCoords).rgb;

	// Discard background pixels that have no geometry (gPosition is sentinel 0,0,0)
	if (length(FragPos) < 0.001 && length(Normal) < 0.001)
		discard;

	vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
	float Specular = texture(gAlbedoSpec, TexCoords).a;

	vec3 viewDir = normalize(viewPos - FragPos);
	vec3 ambient = Diffuse * 0.1f;
	vec3 color = ambient;

	for (int i = 0; i < NR_POINT_LIGHTS; i++)
	{
		color += CalcPointLight(pointLights[i], Normal, FragPos, Diffuse, Specular, viewDir);
	}

	FragColor = vec4(color, 1.0);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 diffuse_arg, float specular_arg, vec3 viewDir)
{
	// diffuse
	vec3 lightDir = normalize(light.position - fragPos);

	float impact = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.color * impact * diffuse_arg;

	// specular
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
	vec3 specular = light.color * spec * specular_arg;

	// attenuation
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (1.0 + light.linear * distance + light.quadratic * (distance * distance));

	diffuse *= attenuation;
	specular *= attenuation;

	return (diffuse + specular);
}
)FSH";