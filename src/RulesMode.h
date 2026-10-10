#pragma once

#include <nlohmann/json.hpp>
#include "Rules.h"

class LoadOrder;

struct RuleParseContext
{
    const std::filesystem::path& rulePath;
    Rule& rule;
    std::string& error;
};

struct RuleApplyContext
{
    const Rule& rule;
    const std::filesystem::path& target;
    const LoadOrder& loadOrder;
    std::string& error;
};

bool ParseRulesMode(const nlohmann::json& json, RuleParseContext& context);
bool ConfigureDefaultTemplateMode(RuleParseContext& context);
bool ConfigureNamedTemplateMode(const std::string& requested, RuleParseContext& context);
bool ApplyRulesMode(RuleApplyContext& context);
