#include <algorithm>
#include <string>
#include <vector>
#include "common/IPrefix.h"

#include "Resolver.h"
#include "FileIO.h"
#include "LoadOrder.h"
#include "Rules.h"
#include "RulesMode.h"
#include "Template.h"

static bool ApplyRule(const Rule& rule, const std::filesystem::path& gameRoot, const LoadOrder& loadOrder);
static bool IsRuleEnabled(const Rule& rule, const LoadOrder& loadOrder);
static bool ApplyReplaceRule(RuleApplyContext& context);
static bool ApplyInsertRule(RuleApplyContext& context);

// Startup flow: reconstruct load order and then apply active rules.
bool ResolveRuntimeFiles(const std::filesystem::path& gameRoot, const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile)
{
    const std::filesystem::path definitionsRoot = gameRoot / "Data" / "RuntimeFileResolver";
    std::string error;
    LoadOrder loadOrder;
    std::vector<Rule> rules;
    bool success = true;

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

    std::stable_sort(rules.begin(), rules.end(), [](const Rule& a, const Rule& b) { return a.priority < b.priority; });

    for (const Rule& rule : rules)
    {
        if (!IsRuleEnabled(rule, loadOrder))
            continue;

        success = ApplyRule(rule, gameRoot, loadOrder) && success;
    }

    return success;
}

static bool IsRuleEnabled(const Rule& rule, const LoadOrder& loadOrder)
{
    if (rule.requiredPlugins.empty())
    {
        if (rule.operation.type == OperationType::Rules && rule.operation.mode == RulesMode::RemoveLines)
            return true;

        return rule.plugin.empty() || loadOrder.Contains(rule.plugin);
    }

    if (rule.requirementMode == PluginRequirementMode::Any)
    {
        for (const std::string& plugin : rule.requiredPlugins)
        {
            if (loadOrder.Contains(plugin))
                return true;
        }

        return false;
    }

    for (const std::string& plugin : rule.requiredPlugins)
    {
        if (!loadOrder.Contains(plugin))
            return false;
    }

    return true;
}

// Builds the target path and dispatches rules
static bool ApplyRule(const Rule& rule, const std::filesystem::path& gameRoot, const LoadOrder& loadOrder)
{
    std::string error;
    const std::filesystem::path target = gameRoot / rule.target;
    RuleApplyContext context{rule, target, loadOrder, error};
    bool success = false;

    switch (rule.operation.type)
    {
    case OperationType::Rules:success = ApplyRulesMode(context);
        break;
    case OperationType::Replace:success = ApplyReplaceRule(context);
        break;
    case OperationType::Append : case OperationType::Prepend:success = ApplyInsertRule(context);
        break;
    }

    if (!success)
    {
        _ERROR("%s: %s", rule.id.c_str(), error.c_str());
        return false;
    }

    _MESSAGE("%s: OK", rule.id.c_str());
    return true;
}

static bool ApplyReplaceRule(RuleApplyContext& context)
{
    std::string current;
    std::string search;
    std::string replacement;
    std::size_t position;

    ReadTextFile(context.target, current);
    ResolveTemplate(context.rule.operation.search, context.loadOrder, search);
    ResolveTemplate(context.rule.operation.replacement, context.loadOrder, replacement);

    position = current.find(search);
    if (position == std::string::npos)
        return true;

    if (context.rule.operation.replaceAll)
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

    return WriteTextFile(context.target, current, context.error);
}


static bool ApplyInsertRule(RuleApplyContext& context)
{
    std::string current;
    std::string text;
    std::string output;

    ReadTextFile(context.target, current);
    ResolveTemplate(context.rule.operation.text, context.loadOrder, text);

    if (context.rule.operation.type == OperationType::Append)
        output = current + text;
    else
        output = text + current;

    return WriteTextFile(context.target, output, context.error);
}
