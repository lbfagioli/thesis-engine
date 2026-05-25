#pragma once

#include <string>

unsigned int getTexture(const std::string& path);
void useTexture(unsigned int textureID, unsigned int unit);
