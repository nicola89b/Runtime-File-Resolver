#pragma once

#include <nlohmann/json.hpp>
#include "LoadOrder.h"
#include "Rules.h"

namespace RuntimeFileResolver
{
    struct RuleParseContext
    {
        const std::filesystem::path& rulePath;
        Rule& rule;
        std::string& error;
    };

    struct RuleApplyContext
    {
        const Rule& rule;
        const Operation& operation;
        const std::filesystem::path& target;
        const ::LoadOrder& loadOrder;
        std::string& error;
    };

    bool ParseRulesMode(const nlohmann::json& json, RuleParseContext& context, Operation& operation);
    bool ConfigureDefaultTemplateMode(RuleParseContext& context, Operation& operation);
    bool ConfigureDefaultRestoreMode(RuleParseContext& context, Operation& operation);
    bool ConfigureNamedTemplateMode(const std::string& requested, RuleParseContext& context, Operation& operation, RulesMode mode = RulesMode::Template);
    bool ApplyRulesMode(RuleApplyContext& context);
    bool ApplyDeleteOperation(RuleApplyContext& context);
}
