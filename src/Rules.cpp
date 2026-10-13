#include "Rules.h"

#include <exception>
#include <nlohmann/json.hpp>
#include <utility>

#include "common/IDirectoryIterator.h"
#include "common/IPrefix.h"
#include "FileIO.h"
#include "RulesMode.h"

namespace RuntimeFileResolver
{
    static bool LoadRuleFile(const std::filesystem::path& path, std::vector<Rule>& rules, std::string& error);
    static bool IsSupportedModeRule(const nlohmann::json& json);
    static bool ParseRule(const nlohmann::json& json, RuleParseContext& context);

    bool LoadRules(const std::filesystem::path& directory, std::vector<Rule>& rules, std::string& error)
    {
        rules.clear();
        error.clear();

        if (!std::filesystem::exists(directory))
        {
            error = "RuntimeFileResolver directory not found";
            return false;
        }

        const std::string path = directory.string();
        for (IDirectoryIterator it(path.c_str(), "*.json"); !it.Done(); it.Next())
        {
            const WIN32_FIND_DATA* entry = it.Get();
            if ((entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;

            if (!LoadRuleFile(it.GetFullPath(), rules, error))
                return false;
        }

        return true;
    }

    static bool LoadRuleFile(const std::filesystem::path& path, std::vector<Rule>& rules, std::string& error)
    {
        try
        {
            std::string text;
            if (!ReadTextFile(path, text, &error))
            {
                error = path.filename().string() + ": " + error;
                return false;
            }

            nlohmann::json json = nlohmann::json::parse(text, nullptr, true, true);
            if (!IsSupportedModeRule(json))
                return true;

            Rule rule;
            RuleParseContext context{path, rule, error};
            if (!ParseRule(json, context))
            {
                error = path.filename().string() + ": " + error;
                return false;
            }

            rules.push_back(std::move(rule));
            return true;
        }
        catch (const std::exception& e)
        {
            error = path.filename().string() + ": " + e.what();
            return false;
        }
    }

    static bool IsSupportedModeRule(const nlohmann::json& json)
    {
        if (!json.is_object() || !json.contains("rules") || !json["rules"].is_object())
            return false;

        const auto& rulesJson = json["rules"];
        return rulesJson.contains("mode") && rulesJson["mode"].is_number_integer() && (rulesJson["mode"] == 0 || rulesJson["mode"] == 1);
    }

    static bool ParseRule(const nlohmann::json& json, RuleParseContext& context)
    {
        Rule& rule = context.rule;
        const auto& rulePath = context.rulePath;

        rule.id = json.value("id", rulePath.stem().string());
        rule.priority = json.value("priority", 0);
        rule.target = json.value("target", "");

        if (json.contains("plugin"))
            rule.plugin = json["plugin"].get<std::string>();

        if (json.contains("requires"))
        {
            const auto& reqs = json["requires"];
            const char* key = nullptr;

            if (reqs.contains("all"))
            {
                key = "all";
                rule.requirementMode = PluginRequirementMode::All;
            }
            else if (reqs.contains("any"))
            {
                key = "any";
                rule.requirementMode = PluginRequirementMode::Any;
            }
            else
            {
                key = "none";
                rule.requirementMode = PluginRequirementMode::None;
            }

            for (const auto& plugin : reqs[key])
                rule.requiredPlugins.push_back(plugin.get<std::string>());
        }

        if (json.contains("files"))
        {
            const auto& files = json["files"];
            const char* key = nullptr;

            if (files.contains("all"))
            {
                key = "all";
                rule.fileRequirementMode = PluginRequirementMode::All;
            }
            else if (files.contains("any"))
            {
                key = "any";
                rule.fileRequirementMode = PluginRequirementMode::Any;
            }
            else
            {
                key = "none";
                rule.fileRequirementMode = PluginRequirementMode::None;
            }

            for (const auto& file : files[key])
                rule.requiredFiles.push_back(file.get<std::string>());
        }

        Operation& modeOp = rule.operations.emplace_back();
        if (!ParseRulesMode(json["rules"], context, modeOp))
            return false;

        if (modeOp.mode == RulesMode::Template)
            return true;

        if (json.contains("operations"))
        {
            for (const auto& item : json["operations"])
            {
                Operation& op = rule.operations.emplace_back();
                if (item.contains("template"))
                {
                    if (!ConfigureNamedTemplateMode(item["template"].get<std::string>(), context, op))
                        return false;
                }
                else if (item.contains("delete"))
                {
                    for (const auto& match : item["delete"])
                        op.lineMatches.push_back(match.get<std::string>());
                    op.type = OperationType::Delete;
                }
                else if (item.contains("replace"))
                {
                    const auto& replace = item["replace"];
                    op.type = OperationType::Replace;
                    op.search = replace.value("search", "");
                    op.replacement = replace.value("with", "");
                    op.replaceAll = replace.value("all", true);
                }
                else if (item.contains("append"))
                {
                    op.type = OperationType::Append;
                    op.text = item["append"].get<std::string>();
                }
                else if (item.contains("prepend"))
                {
                    op.type = OperationType::Prepend;
                    op.text = item["prepend"].get<std::string>();
                }
                else
                {
                    context.error = "operation must contain template, delete, replace, append, or prepend";
                    return false;
                }
            }
        }

        return true;
    }
}
