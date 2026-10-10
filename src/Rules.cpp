#include <exception>
#include <nlohmann/json.hpp>
#include "common/IPrefix.h"
#include "common/IDirectoryIterator.h"

#include "Rules.h"
#include "RulesMode.h"
#include "FileIO.h"


static bool LoadRuleFile(const std::filesystem::path& path, std::vector<Rule>& rules, std::string& error);
static bool ParseRule(const nlohmann::json& json, RuleParseContext& context);
static bool ParsePluginRequirements(const nlohmann::json& json, RuleParseContext& context);
static bool ParseOperation(const nlohmann::json& json, RuleParseContext& context);


// Scans the shared virtual Data\RuntimeFileResolver directory
bool LoadRules(const std::filesystem::path& directory, std::vector<Rule>& rules, std::string& error)
{
    std::string path = directory.string();

    rules.clear();
    error.clear();

    if (!std::filesystem::exists(directory))
    {
        error = "RuntimeFileResolver directory not found";
        return false;
    }

    for (IDirectoryIterator it(path.c_str(), "*.json"); !it.Done(); it.Next())
    {
        WIN32_FIND_DATA* entry = it.Get();
        if ((entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            continue;

        if (!LoadRuleFile(it.GetFullPath(), rules, error))
            return false;
    }

    return true;
}

// Reads and parses one JSON file
static bool LoadRuleFile(const std::filesystem::path& path, std::vector<Rule>& rules, std::string& error)
{
    try
    {
        std::string text;
        ReadTextFile(path, text);

        Rule rule;
        RuleParseContext context{path, rule, error};
        nlohmann::json json = nlohmann::json::parse(text, nlohmann::json::parser_callback_t(), true, true);
        if (!ParseRule(json, context))
        {
            error = path.filename().string() + ": " + error;
            return false;
        }

        rules.push_back(rule);
        return true;
    }
    catch (const std::exception& e)
    {
        error = path.filename().string() + ": " + e.what();
        return false;
    }
}

// Parses the common rule fields and delegates the actual operation payload
static bool ParseRule(const nlohmann::json& json, RuleParseContext& context)
{
    Rule& rule = context.rule;
    const std::filesystem::path& rulePath = context.rulePath;
    std::string& error = context.error;

    if (!json.is_object())
    {
        error = "rule must be a JSON object";
        return false;
    }

    rule.id = json.value("id", rulePath.stem().string());
    rule.priority = json.value("priority", 0);
    rule.target = json.value("target", "");

    if (json.contains("plugin"))
    {
        if (!json["plugin"].is_string())
        {
            error = "plugin must be a string";
            return false;
        }
        rule.plugin = json["plugin"].get<std::string>();
    }

    if (!ParsePluginRequirements(json, context))
        return false;

    if (rule.id.empty())
    {
        error = "rule id is empty";
        return false;
    }
    if (rule.target.empty())
    {
        error = "rule requires target";
        return false;
    }

    return ParseOperation(json, context);
}

static bool ParsePluginRequirements(const nlohmann::json& json, RuleParseContext& context)
{
    Rule& rule = context.rule;
    std::string& error = context.error;

    if (!json.contains("requires")) //cover the case without requires in the INI
        return true;

    const nlohmann::json& requirements = json["requires"];
    const bool hasSingleKey = requirements.is_object() && requirements.size() == 1;
    const bool requireAll = hasSingleKey && requirements.contains("all");
    const bool requireAny = hasSingleKey && requirements.contains("any");
    if (!requireAll && !requireAny)
    {
        error = "requires must contain exactly one all or any list";
        return false;
    }

    rule.requirementMode = requireAll ? PluginRequirementMode::All : PluginRequirementMode::Any;
    const nlohmann::json& plugins = requirements[requireAll ? "all" : "any"];

    if (!plugins.is_array() || plugins.empty())
    {
        error = "requires list must not be empty";
        return false;
    }

    for (const nlohmann::json& plugin : plugins)
    {
        std::string pluginName;
        if (plugin.is_string())
            pluginName = plugin.get<std::string>();

        if (pluginName.empty())
        {
            error = "requires entries must be non-empty plugin names";
            return false;
        }

        rule.requiredPlugins.push_back(pluginName);
    }

    return true;
}


static bool ParseOperation(const nlohmann::json& json, RuleParseContext& context)
{
    Rule& rule = context.rule;
    Operation& operation = rule.operation;
    std::string& error = context.error;

    const bool rulesOp       = json.contains("rules");
    const bool templateOp    = json.contains("template");
    const bool replaceOp     = json.contains("replace");
    const bool appendOp      = json.contains("append");
    const bool prependOp     = json.contains("prepend");
    const int operationCount = templateOp + replaceOp + appendOp + prependOp;
    const char* key;

    if (rulesOp)
    {
        if (operationCount != 0)
        {
            error = "rules cannot be combined with another operation";
            return false;
        }

        return ParseRulesMode(json["rules"], context);
    }

    if (operationCount > 1)
    {
        error = "rule must define only one operation";
        return false;
    }

    if (operationCount == 0)
    {
        return ConfigureDefaultTemplateMode(context);
    }

    if (templateOp)
    {
        if (!json["template"].is_string())
        {
            error = "template must be a string";
            return false;
        }

        return ConfigureNamedTemplateMode(json["template"].get<std::string>(), context);
    }

    if (replaceOp)
    {
        const nlohmann::json& replace = json["replace"];
        if (!replace.is_object())
        {
            error = "replace must be an object";
            return false;
        }

        operation.type = OperationType::Replace;
        operation.search = replace.value("search", "");
        operation.replacement = replace.value("with", "");
        operation.replaceAll = replace.value("all", true);

        if (operation.search.empty())
        {
            error = "replace.search is empty";
            return false;
        }
        return true;
    }

    if (appendOp)
    {
        key = "append";
        operation.type = OperationType::Append;
    }
    else
    {
        key = "prepend";
        operation.type = OperationType::Prepend;
    }

    if (!json[key].is_string())
    {
        error = std::string(key) + " must be a string";
        return false;
    }

    operation.text = json[key].get<std::string>();
    return true;
}
