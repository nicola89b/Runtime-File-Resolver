#pragma once

#include <cstdint>
#include <filesystem>

enum class Runtime : std::uint8_t
{
    SE,
    AE,
    VR,
    Unsupported = 0xFF
};

Runtime DetectRuntime(std::uint32_t runtimeVersion);
const char* RuntimeName(Runtime runtime);
const char* RuntimeSaveFolder(Runtime runtime, std::uint32_t runtimeVersion);
bool FindPluginsFile(Runtime runtime, std::uint32_t runtimeVersion, std::filesystem::path& pluginsFile); 
void FindCreationClubFile(const std::filesystem::path& gameRoot, Runtime runtime, std::filesystem::path& creationClubFile);
