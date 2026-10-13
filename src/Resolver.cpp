#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>
#include "common/IPrefix.h"

#include "Resolver.h"
#include "FileIO.h"
#include "LoadOrder.h"
#include "Rules.h"
#include "RulesMode.h"
#include "Template.h"

namespace RuntimeFileResolver
{
    static bool ApplyRule(const Rule& rule, const std::filesystem::path& gameRoot, const LoadOrder& loadOrder, bool hasRestoreMode, bool applyRestore, bool& restoreCompleted);
    static bool RuleRequirementsMet(const Rule& rule, const std::filesystem::path& dataDirectory, const LoadOrder& loadOrder);
    static bool IsDataFile(const std::filesystem::path& dataDirectory, const std::filesystem::path& relativePath);
    static bool IsRestoreModeRule(const Rule& rule);
    static bool ApplyReplaceRule(RuleApplyContext& context);
    static bool ApplyInsertRule(RuleApplyContext& context);
    static bool ResolveRuntimeFilesImpl(const std::filesystem::path& gameRoot, const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile);

    bool ResolveRuntimeFiles(const std::filesystem::path& gameRoot, const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile)
    {
        try
        {
            return ResolveRuntimeFilesImpl(gameRoot, pluginsFile, creationClubFile);
        }
        catch (const std::exception& e)
        {
            _ERROR("resolver: %s", e.what());
            return false;
        }
    }

    static bool ResolveRuntimeFilesImpl(const std::filesystem::path& gameRoot, const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile)
    {
        const std::filesystem::path definitionsRoot = gameRoot / "Data" / "RuntimeFileResolver";
        std::string error;
        LoadOrder loadOrder;
        std::vector<Rule> rules;
        if (!loadOrder.Build(pluginsFile, creationClubFile, gameRoot / "Data", error))
        {
            _ERROR("load order: %s", error.c_str());
            return false;
        }
        _MESSAGE("Plugins: %zu", loadOrder.Count());

        if (!LoadRules(definitionsRoot, rules, error))
        {
            _ERROR("rules: %s", error.c_str());
            return false;
        }

        std::stable_sort(rules.begin(), rules.end(), [](const Rule& a, const Rule& b)
        {
            if (IsRestoreModeRule(a) != IsRestoreModeRule(b))
                return IsRestoreModeRule(a);
            return a.priority < b.priority;
        });

        bool success = true;
        std::vector<std::filesystem::path> restoredTargets;
        for (const Rule& rule : rules)
        {
            const bool hasRestoreMode = IsRestoreModeRule(rule);
            const std::filesystem::path target = (gameRoot / rule.target).lexically_normal();
            const bool applyRestore = hasRestoreMode && std::find(restoredTargets.begin(), restoredTargets.end(), target) == restoredTargets.end();
            bool restoreCompleted = false;
            const bool ruleSuccess = ApplyRule(rule, gameRoot, loadOrder, hasRestoreMode, applyRestore, restoreCompleted);

            if (restoreCompleted)
                restoredTargets.push_back(target);

            success = ruleSuccess && success;
        }

        return success;
    }

    static bool RuleRequirementsMet(const Rule& rule, const std::filesystem::path& dataDirectory, const LoadOrder& loadOrder)
    {
        bool pluginRequirementsMet = true;
        if (rule.requiredPlugins.empty())
        {
            pluginRequirementsMet = rule.plugin.empty() || loadOrder.IsExplicitlyEnabled(rule.plugin);
        }
        else if (rule.requirementMode == PluginRequirementMode::Any)
        {
            pluginRequirementsMet = false;
            for (const std::string& plugin : rule.requiredPlugins)
            {
                if (loadOrder.IsExplicitlyEnabled(plugin))
                {
                    pluginRequirementsMet = true;
                    break;
                }
            }
        }
        else if (rule.requirementMode == PluginRequirementMode::None)
        {
            for (const std::string& plugin : rule.requiredPlugins)
            {
                if (loadOrder.IsExplicitlyEnabled(plugin))
                {
                    pluginRequirementsMet = false;
                    break;
                }
            }
        }
        else
        {
            for (const std::string& plugin : rule.requiredPlugins)
            {
                if (!loadOrder.IsExplicitlyEnabled(plugin))
                {
                    pluginRequirementsMet = false;
                    break;
                }
            }
        }

        if (!pluginRequirementsMet || rule.requiredFiles.empty())
            return pluginRequirementsMet;

        if (rule.fileRequirementMode == PluginRequirementMode::Any)
        {
            for (const std::filesystem::path& file : rule.requiredFiles)
            {
                if (IsDataFile(dataDirectory, file))
                    return true;
            }

            return false;
        }

        if (rule.fileRequirementMode == PluginRequirementMode::None)
        {
            for (const std::filesystem::path& file : rule.requiredFiles)
            {
                if (IsDataFile(dataDirectory, file))
                    return false;
            }

            return true;
        }

        for (const std::filesystem::path& file : rule.requiredFiles)
        {
            if (!IsDataFile(dataDirectory, file))
                return false;
        }

        return true;
    }

    static bool IsDataFile(const std::filesystem::path& dataDirectory, const std::filesystem::path& relativePath)
    {
        if (relativePath.empty() || relativePath.has_root_path())
            return false;

        const auto base = dataDirectory.lexically_normal();
        const auto candidate = (base / relativePath).lexically_normal();
        const auto back = candidate.lexically_relative(base);
        if (back.empty() || *back.begin() == "..")
            return false;

        std::error_code ec;
        return std::filesystem::is_regular_file(candidate, ec);
    }

    static bool IsRestoreModeRule(const Rule& rule)
    {
        for (const Operation& operation : rule.operations)
        {
            if (operation.type == OperationType::Rules && operation.mode == RulesMode::Restore)
                return true;
        }

        return false;
    }

    static bool ApplyRule(const Rule& rule, const std::filesystem::path& gameRoot, const LoadOrder& loadOrder, bool hasRestoreMode, bool applyRestore, bool& restoreCompleted)
    {
        std::string error;
        const std::filesystem::path target = gameRoot / rule.target;
        const bool requirementsMet = RuleRequirementsMet(rule, gameRoot / "Data", loadOrder);
        restoreCompleted = false;

        if (!hasRestoreMode && !requirementsMet)
            return true;

        for (const Operation& operation : rule.operations)
        {
            const bool isRestoreOperation = operation.type == OperationType::Rules && operation.mode == RulesMode::Restore;
            if (isRestoreOperation && !applyRestore)
                continue;
            if (!requirementsMet && !isRestoreOperation)
                continue;

            RuleApplyContext context{rule, operation, target, loadOrder, error};
            bool success = false;
            switch (operation.type)
            {
            case OperationType::Rules: success = ApplyRulesMode(context); break;
            case OperationType::Delete: success = ApplyDeleteOperation(context); break;
            case OperationType::Replace: success = ApplyReplaceRule(context); break;
            case OperationType::Append:
            case OperationType::Prepend: success = ApplyInsertRule(context); break;
            }

            if (!success)
            {
                _ERROR("%s: %s", rule.id.c_str(), error.c_str());
                return false;
            }

            if (isRestoreOperation)
                restoreCompleted = true;
        }

        _MESSAGE("%s: OK", rule.id.c_str());
        return true;
    }

    static bool ApplyReplaceRule(RuleApplyContext& context)
    {
        std::string current;
        std::string search;
        std::string replacement;
        if (!ReadTextFile(context.target, current, &context.error))
            return false;

        ResolveTemplate(context.operation.search, context.loadOrder, search);
        ResolveTemplate(context.operation.replacement, context.loadOrder, replacement);

        if (search.empty())
        {
            context.error = "search resolves to an empty string";
            return false;
        }

        std::size_t position = current.find(search);
        if (position == std::string::npos)
            return true;

        if (context.operation.replaceAll)
        {
            do
            {
                current.replace(position, search.size(), replacement);
                position = current.find(search, position + replacement.size());
            } while (position != std::string::npos);
        }
        else
        {
            current.replace(position, search.size(), replacement);
        }

        return WriteTextFile(context.target, current, &context.error);
    }


    static bool ApplyInsertRule(RuleApplyContext& context)
    {
        std::string current;
        std::string text;
        if (!ReadTextFile(context.target, current, &context.error))
            return false;

        ResolveTemplate(context.operation.text, context.loadOrder, text);

        std::string output;
        if (context.operation.type == OperationType::Append)
            output = current + text;
        else
            output = text + current;

        return WriteTextFile(context.target, output, &context.error);
    }
}
