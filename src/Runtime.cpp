#include <ShlObj.h>
#include "skse64_common/skse_version.h"

#include "Runtime.h"

// Maps SKSE runtime version.
Runtime DetectRuntime(std::uint32_t runtimeVersion)
{
    if (runtimeVersion == MAKE_EXE_VERSION_EX(1, 4, 15, 1))
        return Runtime::VR;

    if (GET_EXE_VERSION_MAJOR(runtimeVersion) != 1)
        return Runtime::Unsupported;

    if (GET_EXE_VERSION_MINOR(runtimeVersion) <= 5)
        return Runtime::SE;
    else
        return Runtime::AE;
}

const char* RuntimeName(Runtime runtime)
{
    switch (runtime) 
    {
        case Runtime::SE: return "SE";
        case Runtime::AE: return "AE";
        case Runtime::VR: return "VR";
        default: return "Unsupported";
    }
}

const char* RuntimeSaveFolder(Runtime runtime, std::uint32_t runtimeVersion)
{
    if (runtime == Runtime::VR)
    {
        return "Skyrim VR";
    }
    else
    {
        switch (GET_EXE_VERSION_SUB(runtimeVersion))
        {
            case RUNTIME_TYPE_GOG: return "Skyrim Special Edition GOG";
            case RUNTIME_TYPE_EPIC: return "Skyrim Special Edition EPIC";
            default: return "Skyrim Special Edition";
        }
    }
}

// Resolves plugins.txt
bool FindPluginsFile(Runtime runtime, std::uint32_t runtimeVersion, std::filesystem::path& pluginsFile)
{
    char localAppData[MAX_PATH] = {};
    std::filesystem::path path;

    pluginsFile.clear();
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, localAppData)))
        return false;

    path = std::filesystem::path(localAppData) / RuntimeSaveFolder(runtime, runtimeVersion) / "plugins.txt";
    if (!std::filesystem::exists(path))
        return false;

    pluginsFile = path;
    return true;
}


// Finds Skyrim.ccc on flat runtimes
void FindCreationClubFile(const std::filesystem::path& gameRoot, Runtime runtime, std::filesystem::path& creationClubFile)
{
    std::filesystem::path path;
    creationClubFile.clear();
    if (runtime != Runtime::VR)
    {
        path = gameRoot / "Skyrim.ccc";
        if (std::filesystem::exists(path))
            creationClubFile = path;
    }
}
