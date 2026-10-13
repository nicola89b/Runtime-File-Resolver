#include <ShlObj.h>
#include <utility>
#include "skse64_common/skse_version.h"
#include "Runtime.h"

Runtime DetectRuntime(std::uint32_t runtimeVersion)
{
    if (runtimeVersion == MAKE_EXE_VERSION_EX(1, 4, 15, 1))
        return Runtime::VR;

    if (GET_EXE_VERSION_MAJOR(runtimeVersion) != 1)
        return Runtime::Unsupported;

    return GET_EXE_VERSION_MINOR(runtimeVersion) <= 5 ? Runtime::SE : Runtime::AE;
}

const char* RuntimeName(Runtime runtime)
{
    switch (runtime) {
        case Runtime::SE: return "SE";
        case Runtime::AE: return "AE";
        case Runtime::VR: return "VR";
        default:          return "Unsupported";
    }
}

const char* RuntimeSaveFolder(Runtime runtime, std::uint32_t runtimeVersion)
{
    if (runtime == Runtime::VR)
        return "Skyrim VR";

    switch (GET_EXE_VERSION_SUB(runtimeVersion)) {
        case RUNTIME_TYPE_GOG:  return "Skyrim Special Edition GOG";
        case RUNTIME_TYPE_EPIC: return "Skyrim Special Edition EPIC";
        default:                return "Skyrim Special Edition";
    }
}

bool FindPluginsFile(Runtime runtime, std::uint32_t runtimeVersion, std::filesystem::path& pluginsFile)
{
    pluginsFile.clear();

    char localAppData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData)))
        return false;

    auto path = std::filesystem::path(localAppData) / RuntimeSaveFolder(runtime, runtimeVersion) / "plugins.txt";
    if (!std::filesystem::exists(path))
        return false;

    pluginsFile = std::move(path);
    return true;
}

void FindCreationClubFile(const std::filesystem::path& gameRoot, Runtime runtime, std::filesystem::path& creationClubFile)
{
    creationClubFile.clear();
    if (runtime == Runtime::VR)
        return;

    auto path = gameRoot / "Skyrim.ccc";
    if (std::filesystem::exists(path))
        creationClubFile = std::move(path);
}
