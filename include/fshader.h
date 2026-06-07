#pragma once

#include <string>

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

// Unified Forward Shader: point lights only, texture reads hoisted outside loop.
// Lighting model matches Deferred screenFSH exactly: global ambient + per-light diffuse/specular.
inline const char* cubeFSH = R"FSH(
#version 330 core

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

out vec4 FragColor;

struct Material {
	sampler2D texture_diffuse1;
	sampler2D texture_specular1;
	float shininess;
};

struct PointLight {
	vec3 position;
	vec3 color;       // diffuse & specular colour
	float linear;
	float quadratic;
};

#define NR_POINT_LIGHTS 4

uniform Material material;
uniform vec3 viewPos;
uniform PointLight pointLights[NR_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                    vec3 diffuseTex, float specularTex);

void main()
{
	vec3 norm = normalize(Normal);
	vec3 viewDir = normalize(viewPos - FragPos);

	// --- Hoist texture reads OUTSIDE the per-light loop ---
	// This guarantees O(1) texture fetches regardless of NR_POINT_LIGHTS,
	// making the comparison fair and portable across GPU vendors.
	vec3 diffuseTex  = texture(material.texture_diffuse1,  TexCoords).rgb;
	float specularTex = texture(material.texture_specular1, TexCoords).r;

	// Global ambient (identical to Deferred screenFSH)
	vec3 ambient = diffuseTex * 0.05;
	vec3 color = ambient;

	for (int i = 0; i < NR_POINT_LIGHTS; i++)
		color += CalcPointLight(pointLights[i], norm, FragPos, viewDir,
		                        diffuseTex, specularTex);

	FragColor = vec4(color, 1.0);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                    vec3 diffuseTex, float specularTex)
{
	vec3 lightDir = normalize(light.position - fragPos);

	// diffuse
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.color * diff * diffuseTex;

	// specular (scalar specular, matching Deferred)
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
	vec3 specular = light.color * spec * specularTex;

	// attenuation (constant = 1.0, matching Deferred)
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (1.0 + light.linear * distance + light.quadratic * (distance * distance));

	return (diffuse + specular) * attenuation;
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

// Unified Deferred screen-space shader: matches Forward cubeFSH lighting model.
// Global ambient 0.05 + per-light diffuse/specular with attenuation.
inline const char* screenFSH = R"FSH(
#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

struct PointLight {
	vec3 position;
	vec3 color;       // diffuse & specular colour
	float linear;
	float quadratic;
};

#define NR_POINT_LIGHTS 4

uniform vec3 viewPos;
uniform PointLight pointLights[NR_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                    vec3 diffuse_arg, float specular_arg);

void main()
{
	vec3 FragPos = texture(gPosition, TexCoords).rgb;
	vec3 Normal  = texture(gNormal,   TexCoords).rgb;

	// Discard background pixels that have no geometry (gPosition is sentinel 0,0,0)
	if (length(FragPos) < 0.001 && length(Normal) < 0.001)
		discard;

	vec3 Diffuse  = texture(gAlbedoSpec, TexCoords).rgb;
	float Specular = texture(gAlbedoSpec, TexCoords).a;

	vec3 viewDir = normalize(viewPos - FragPos);

	// Global ambient — matches Forward cubeFSH
	vec3 ambient = Diffuse * 0.05;
	vec3 color = ambient;

	for (int i = 0; i < NR_POINT_LIGHTS; i++)
		color += CalcPointLight(pointLights[i], Normal, FragPos, viewDir,
		                        Diffuse, Specular);

	FragColor = vec4(color, 1.0);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir,
                    vec3 diffuse_arg, float specular_arg)
{
	vec3 lightDir = normalize(light.position - fragPos);

	// diffuse
	float diff = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = light.color * diff * diffuse_arg;

	// specular (scalar, matching Forward)
	vec3 reflectDir = reflect(-lightDir, normal);
	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
	vec3 specular = light.color * spec * specular_arg;

	// attenuation (constant = 1.0, matching Forward)
	float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (1.0 + light.linear * distance + light.quadratic * (distance * distance));

	return (diffuse + specular) * attenuation;
}
)FSH";

// Dynamic shader source generators: replace NR_POINT_LIGHTS at runtime.
// Uses string search-and-replace on the inline shader sources above.
// These allocate a full copy — acceptable since shader compilation is
// a one-time cost per experiment configuration, not per frame.

inline std::string getCubeFSH(int lightCount)
{
	   std::string src = cubeFSH;
	   std::string oldDef = "#define NR_POINT_LIGHTS 4";
	   std::string newDef = "#define NR_POINT_LIGHTS " + std::to_string(lightCount);
	   size_t pos = src.find(oldDef);
	   if (pos != std::string::npos)
	       src.replace(pos, oldDef.length(), newDef);
	   // DIAGNOSTIC: Print first ~120 chars of the shader source to verify NR_POINT_LIGHTS
	   std::cout << "[DIAG getCubeFSH(" << lightCount << ")] src snippet: \""
	             << src.substr(0, 120) << "...\"" << std::endl;
	   return src;
}

inline std::string getScreenFSH(int lightCount)
{
	   std::string src = screenFSH;
	   std::string oldDef = "#define NR_POINT_LIGHTS 4";
	   std::string newDef = "#define NR_POINT_LIGHTS " + std::to_string(lightCount);
	   size_t pos = src.find(oldDef);
	   if (pos != std::string::npos)
	       src.replace(pos, oldDef.length(), newDef);
	   // DIAGNOSTIC: Print first ~120 chars of the shader source to verify NR_POINT_LIGHTS
	   std::cout << "[DIAG getScreenFSH(" << lightCount << ")] src snippet: \""
	             << src.substr(0, 120) << "...\"" << std::endl;
	   return src;
}