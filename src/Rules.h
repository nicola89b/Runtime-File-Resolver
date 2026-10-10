#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class OperationType
{
    Rules,
    Replace,
    Append,
    Prepend
};

enum class RulesMode
{
    Template = 0,
    RemoveLines = 1
};

enum class PluginRequirementMode
{
    All,
    Any
};

struct Operation
{
    OperationType type;
    RulesMode mode;
    std::filesystem::path source;
    std::filesystem::path backupDirectory;
    std::string search;
    std::string replacement;
    std::string text;
    bool replaceAll;

    Operation() : type(OperationType::Rules), mode(RulesMode::Template), replaceAll(true) {}
};

struct Rule
{
    std::string id;
    int priority;
    std::string plugin;
    std::vector<std::string> requiredPlugins;
    PluginRequirementMode requirementMode;
    std::filesystem::path target;
    Operation operation;
    Rule() : priority(0), requirementMode(PluginRequirementMode::All) {}
};

bool LoadRules(const std::filesystem::path& directory, std::vector<Rule>& rules, std::string& error);
