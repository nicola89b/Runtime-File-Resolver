#include <chrono>
#include <sstream>
#include <string_view>

#include "RulesMode.h"
#include "FileIO.h"
#include "LoadOrder.h"
#include "Template.h"



static std::filesystem::path ResolveTemplatePath(const std::filesystem::path& templatesDirectory, const std::string& requested);
static bool ApplyTemplateMode(RuleApplyContext& context);
static bool ApplyRemoveLinesMode(RuleApplyContext& context);

bool ParseRulesMode(const nlohmann::json& json, RuleParseContext& context)
{
    const std::filesystem::path& rulePath = context.rulePath;
    const Rule& rule = context.rule;
    Operation& operation = context.rule.operation;
    std::string& error = context.error;

    if (!json.is_object() || !json.contains("mode") || !json["mode"].is_number_integer() || (json["mode"] != 0 && json["mode"] != 1))
    {
        error = "rules must be 0 or 1";
        return false;
    }

    if (json["mode"] == 0)
    {
        if (json.contains("template"))
            return ConfigureNamedTemplateMode(json["template"].get<std::string>(), context);
        return ConfigureDefaultTemplateMode(context);
    }

    if (rule.plugin.empty())
    {
        error = "rules.mode 1 requires plugin";
        return false;
    }

    if (json.contains("template"))
    {
        error = "rules.template is only valid with mode 0";
        return false;
    }

    operation.type = OperationType::Rules;
    operation.mode = RulesMode::RemoveLines;
    operation.backupDirectory = rulePath.parent_path() / "backups";
    return true;
}

bool ConfigureDefaultTemplateMode(RuleParseContext& context)
{
    const std::filesystem::path& rulePath = context.rulePath;
    Operation& operation = context.rule.operation;
    std::string& error = context.error;
    const std::filesystem::path templatesDirectory = rulePath.parent_path() / "Templates";

    operation.type = OperationType::Rules;
    operation.mode = RulesMode::Template;
    operation.source = templatesDirectory / rulePath.filename();
    operation.source.replace_extension(".ini");
    if (!std::filesystem::exists(operation.source))
    {
        error = "template was not found in Templates folder: " + operation.source.filename().string();
        return false;
    }

    return true;
}

bool ConfigureNamedTemplateMode(const std::string& requested, RuleParseContext& context)
{
    const std::filesystem::path& rulePath = context.rulePath;
    Operation& operation = context.rule.operation;
    std::string& error = context.error;
    const std::filesystem::path templatesDirectory = rulePath.parent_path() / "Templates";
    operation.type = OperationType::Rules;
    operation.mode = RulesMode::Template;
    operation.source = ResolveTemplatePath(templatesDirectory, requested);
    if (operation.source.empty())
    {
        error = "template path must be inside Templates folder";
        return false;
    }

    return true;
}

bool ApplyRulesMode(RuleApplyContext& context)
{
    switch (context.rule.operation.mode)
    {
    case RulesMode::Template: return ApplyTemplateMode(context);
    case RulesMode::RemoveLines: return ApplyRemoveLinesMode(context);
    }

    return false;
}

static bool ApplyTemplateMode(RuleApplyContext& context)
{
    std::string source;
    std::string output;
    ReadTextFile(context.rule.operation.source, source);
    ResolveTemplate(source, context.loadOrder, output);
    return WriteTextFile(context.target, output, context.error);
}

static bool ApplyRemoveLinesMode(RuleApplyContext& context)
{
    std::string current;
    std::string output;
    std::string backupError;
    std::size_t start = 0;
    bool removed = false;

    ReadTextFile(context.target, current);

    output.reserve(current.size());
    while (start < current.size())
    {
        const std::size_t lineEnd = current.find_first_of("\r\n", start);
        const std::size_t contentEnd = lineEnd == std::string::npos ? current.size() : lineEnd;
        std::size_t next = contentEnd;

        if (lineEnd != std::string::npos)
        {
            ++next;
            if (current[lineEnd] == '\r' && next < current.size() && current[next] == '\n')
                ++next;
        }

        const std::string_view line(current.data() + start, contentEnd - start);
        if (line.find(context.rule.plugin) == std::string_view::npos)
            output.append(current, start, next - start);
        else
            removed = true;

        start = next;
    }

    if (!removed)
        return true;

    const auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::filesystem::path backupPath = context.rule.operation.backupDirectory / context.target.filename();
    backupPath += "." + std::to_string(timestamp) + ".bak";
    if (!WriteTextFile(backupPath, current, backupError))
    {
        context.error = "cannot create Mode 1 backup (" + backupError + ")";
        return false;
    }

    return WriteTextFile(context.target, output, context.error);
}

static std::filesystem::path ResolveTemplatePath(const std::filesystem::path& templatesDirectory, const std::string& requested)
{
    const std::filesystem::path relative(requested);
    std::filesystem::path base;
    std::filesystem::path candidate;
    std::filesystem::path back;
    base = templatesDirectory.lexically_normal();
    candidate = (base / relative).lexically_normal();
    back = candidate.lexically_relative(base);

    if (!relative.empty() && !relative.has_root_path() && (back.begin() == back.end() || *back.begin() != ".."))
        return candidate;

    return std::filesystem::path();
}
