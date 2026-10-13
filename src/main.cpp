#include <ShlObj.h>
#include <filesystem>
#include <string>

#include "common/IPrefix.h"
#include "skse64/PluginAPI.h"
#include "skse64_common/Utilities.h"
#include "skse64_common/skse_version.h"

#include "Resolver.h"
#include "Runtime.h"

IDebugLog gLog;

extern "C"
{
    __declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
    {
        SKSEPluginVersionData::kVersion,
        130,
        {"Runtime File Resolver"},
        {"nicola89b"},
        {""},
        SKSEPluginVersionData::kVersionIndependentEx_NoStructUse,
        SKSEPluginVersionData::kVersionIndependent_Signatures,
        {0},
        0
    };

    __declspec(dllexport) bool SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info)
    {
        const auto runtime = DetectRuntime(skse->runtimeVersion);
        const std::string logPath = "\\My Games\\" + std::string(RuntimeSaveFolder(runtime, skse->runtimeVersion)) + "\\SKSE\\RuntimeFileResolver.log";

        gLog.OpenRelative(CSIDL_MYDOCUMENTS, logPath.c_str());
        _MESSAGE("Runtime File Resolver");

        info->infoVersion = PluginInfo::kInfoVersion;
        info->name = "Runtime File Resolver";
        info->version = 130;

        if (runtime != Runtime::Unsupported)
            return true;

        _MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);
        return false;
    }

    __declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface* skse)
    {
        const auto runtime = DetectRuntime(skse->runtimeVersion);
        if (runtime == Runtime::Unsupported)
            return false;

        _MESSAGE("%s 1.3 loaded - %s - Version %d.%d.%d",
            "Runtime File Resolver", RuntimeName(runtime),
            GET_EXE_VERSION_MAJOR(skse->runtimeVersion),
            GET_EXE_VERSION_MINOR(skse->runtimeVersion),
            GET_EXE_VERSION_BUILD(skse->runtimeVersion)
        );

        std::filesystem::path pluginsFile;
        if (FindPluginsFile(runtime, skse->runtimeVersion, pluginsFile))
        {
            const auto gameRoot = GetRuntimeDirectory();
            std::filesystem::path creationClubFile;
            FindCreationClubFile(gameRoot, runtime, creationClubFile);
            RuntimeFileResolver::ResolveRuntimeFiles(gameRoot, pluginsFile, creationClubFile);
        }
        else
        {
            _ERROR("plugins not found");
        }

        return true;
    }
}
