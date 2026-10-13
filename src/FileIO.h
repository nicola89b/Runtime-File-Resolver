#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace RuntimeFileResolver
{
    bool ReadTextFile(const std::filesystem::path& path, std::string& text, std::string* outError = nullptr);
    bool WriteTextFile(const std::filesystem::path& path, std::string_view text, std::string* outError = nullptr);
}
