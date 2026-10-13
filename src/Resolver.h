#pragma once

#include <filesystem>

namespace RuntimeFileResolver
{
    bool ResolveRuntimeFiles(const std::filesystem::path& gameRoot, const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile);
}
