#include "RulesMode.h"

#include <filesystem>
#include <string_view>

#include "FileIO.h"
#include "Template.h"

namespace RuntimeFileResolver
{
    static std::filesystem::path ResolveTemplatePath(const std::filesystem::path& templatesDirectory, const std::string& requested)
    {
        const std::filesystem::path relative(requested);
        const auto base = templatesDirectory.lexically_normal();
        const auto candidate = (base / relative).lexically_normal();
        const auto back = candidate.lexically_relative(base);

        if (!relative.empty() && !relative.has_root_path() && (back.begin() == back.end() || *back.begin() != ".."))
            return candidate;

        return {};
    }

    bool ParseRulesMode(const nlohmann::json& json, RuleParseContext& context, Operation& operation)
    {
        if (!json.is_object() || !json.contains("mode") || !json["mode"].is_number_integer() || (json["mode"] != 0 && json["mode"] != 1))
        {
            context.error = "mode must be 0 or 1";
            return false;
        }

        if (json.contains("remove"))
        {
            context.error = "line filters belong in an ordered delete operation";
            return false;
        }

        const bool hasTemplate = json.contains("template");
        const int mode = json["mode"].get<int>();

        if (mode == 0)
        {
            if (hasTemplate)
                return ConfigureNamedTemplateMode(json["template"].get<std::string>(), context, operation);
            return ConfigureDefaultTemplateMode(context, operation);
        }

        if (hasTemplate)
            return ConfigureNamedTemplateMode(json["template"].get<std::string>(), context, operation, RulesMode::Restore);

        return ConfigureDefaultRestoreMode(context, operation);
    }

    bool ConfigureDefaultTemplateMode(RuleParseContext& context, Operation& operation)
    {
        std::filesystem::path templateName = context.rulePath.filename();
        templateName.replace_extension(".ini");
        return ConfigureNamedTemplateMode(templateName.string(), context, operation);
    }

    bool ConfigureDefaultRestoreMode(RuleParseContext& context, Operation& operation)
    {
        const auto templatesDirectory = context.rulePath.parent_path() / "Templates";
        operation.type = OperationType::Rules;
        operation.mode = RulesMode::Restore;
        operation.source = templatesDirectory / context.rule.target.filename();
        return true;
    }

    bool ConfigureNamedTemplateMode(const std::string& requested, RuleParseContext& context, Operation& operation, RulesMode mode)
    {
        const auto templatesDirectory = context.rulePath.parent_path() / "Templates";
        operation.type = OperationType::Rules;
        operation.mode = mode;
        operation.source = ResolveTemplatePath(templatesDirectory, requested);

        if (operation.source.empty())
            return false;

        if (mode == RulesMode::Template && !std::filesystem::exists(operation.source))
        {
            context.error = "template was not found in Templates folder: " + operation.source.filename().string();
            return false;
        }

        return true;
    }

    static bool ApplyTemplateMode(RuleApplyContext& context)
    {
        std::string source;
        if (!ReadTextFile(context.operation.source, source, &context.error))
            return false;

        std::string output;
        ResolveTemplate(source, context.loadOrder, output);

        return WriteTextFile(context.target, output, &context.error);
    }

    static bool RemoveMatchingLines(std::string_view current, const std::vector<std::string>& searches, std::string& output)
    {
        std::size_t start = 0;
        bool removed = false;

        output.reserve(current.size());
        while (start < current.size())
        {
            const std::size_t lineEnd = current.find_first_of("\r\n", start);
            const std::size_t contentEnd = lineEnd == std::string_view::npos ? current.size() : lineEnd;
            std::size_t next = contentEnd;

            if (lineEnd != std::string_view::npos)
            {
                ++next;
                if (current[lineEnd] == '\r' && next < current.size() && current[next] == '\n')
                    ++next;
            }

            const std::string_view line = current.substr(start, contentEnd - start);
            bool matches = false;
            for (const std::string& search : searches)
            {
                if (line.find(search) != std::string_view::npos)
                {
                    matches = true;
                    break;
                }
            }

            if (!matches)
                output.append(current.substr(start, next - start));
            else
                removed = true;

            start = next;
        }

        return removed;
    }

    bool ApplyDeleteOperation(RuleApplyContext& context)
    {
        std::string current;
        if (!ReadTextFile(context.target, current, &context.error))
            return false;

        std::string output;
        if (RemoveMatchingLines(current, context.operation.lineMatches, output))
            return WriteTextFile(context.target, output, &context.error);

        return true;
    }

    static bool ApplyRestoreMode(RuleApplyContext& context)
    {
        const auto& sourcePath = context.operation.source;
        std::string source;

        if (std::filesystem::exists(sourcePath))
        {
            if (!ReadTextFile(sourcePath, source, &context.error))
                return false;
        }
        else
        {
            if (!std::filesystem::exists(context.target))
            {
                context.error = "Mode 1 cannot find the target to preserve as its original template";
                return false;
            }

            if (!ReadTextFile(context.target, source, &context.error))
                return false;

            if (!WriteTextFile(sourcePath, source, &context.error))
                return false;
        }

        return WriteTextFile(context.target, source, &context.error);
    }

    bool ApplyRulesMode(RuleApplyContext& context)
    {
        switch (context.operation.mode)
        {
        case RulesMode::Template: return ApplyTemplateMode(context);
        case RulesMode::Restore: return ApplyRestoreMode(context);
        }

        return false;
    }
}
