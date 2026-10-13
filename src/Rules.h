#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace RuntimeFileResolver
{
    enum class OperationType
    {
        Rules,
        Delete,
        Replace,
        Append,
        Prepend
    };

    enum class RulesMode
    {
        Template = 0,
        Restore = 1
    };

    enum class PluginRequirementMode
    {
        All,
        Any,
        None
    };

    struct Operation
    {
        OperationType type{OperationType::Rules};
        RulesMode mode{RulesMode::Template};
        std::filesystem::path source;
        std::vector<std::string> lineMatches;
        std::string search;
        std::string replacement;
        std::string text;
        bool replaceAll{true};
    };

    struct Rule
    {
        std::string id;
        int priority{0};
        std::string plugin;
        std::vector<std::string> requiredPlugins;
        PluginRequirementMode requirementMode{PluginRequirementMode::All};
        std::vector<std::filesystem::path> requiredFiles;
        PluginRequirementMode fileRequirementMode{PluginRequirementMode::All};
        std::filesystem::path target;
        std::vector<Operation> operations;
    };

    bool LoadRules(const std::filesystem::path& directory, std::vector<Rule>& rules, std::string& error);
}
