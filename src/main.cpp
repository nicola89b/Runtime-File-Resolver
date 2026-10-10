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
        100,
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
        Runtime runtime = DetectRuntime(skse->runtimeVersion);
        std::string logPath;
        logPath = std::string("\\My Games\\") + RuntimeSaveFolder(runtime, skse->runtimeVersion) + "\\SKSE\\RuntimeFileResolver.log";
        gLog.OpenRelative(CSIDL_MYDOCUMENTS, logPath.c_str());
        _MESSAGE("Runtime File Resolver");

        info->infoVersion = PluginInfo::kInfoVersion;
        info->name = "Runtime File Resolver";
        info->version = 100;

        if (runtime != Runtime::Unsupported)
            return true;

        _MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);
        return false;
    }

    // Resolves all registered runtime files once during startup
    __declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface* skse)
    {

        Runtime runtime = DetectRuntime(skse->runtimeVersion);
        std::filesystem::path gameRoot;
        std::filesystem::path pluginsFile;
        std::filesystem::path creationClubFile;
        if (runtime == Runtime::Unsupported)
            return false;

        _MESSAGE("%s 1.0 loaded - %s - Version %d.%d.%d",
                "Runtime File Resolver", RuntimeName(runtime),
                GET_EXE_VERSION_MAJOR(skse->runtimeVersion), 
                GET_EXE_VERSION_MINOR(skse->runtimeVersion),
                GET_EXE_VERSION_BUILD(skse->runtimeVersion)
                );

        gameRoot = GetRuntimeDirectory();
        if (FindPluginsFile(runtime, skse->runtimeVersion, pluginsFile)) 
        {
            FindCreationClubFile(gameRoot, runtime, creationClubFile);
            ResolveRuntimeFiles(gameRoot, pluginsFile, creationClubFile);
        }
        else { _ERROR("plugins not found"); }
        return true;
    }
}
