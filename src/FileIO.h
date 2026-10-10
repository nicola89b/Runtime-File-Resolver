#pragma once

#include <filesystem>
#include <string>

void ReadTextFile(const std::filesystem::path& path, std::string& text);
bool WriteTextFile(const std::filesystem::path& path, const std::string& text, std::string& error); 
