#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>

#include "Template.h"
#include "LoadOrder.h"

static void ParseHex(const std::string& text, std::uint32_t& value);
static void ParseTrimCount(const std::string& format, std::size_t& trimCount);
static std::string ResolveForm(const LoadedPlugin& plugin, std::uint32_t local, std::size_t trimCount);

void ResolveTemplate(const std::string& input, const LoadOrder& loadOrder, std::string& output)
{
    constexpr std::string_view kPluginPrefix = "{{PLUGIN_INDEX:";
    constexpr std::string_view kFormPrefix = "{{FORM:";

    std::size_t cursor = 0;
    output.clear();
    output.reserve(input.size() + 64);

    for (;;)
    {
        const std::size_t pluginIndexStart = input.find(kPluginPrefix, cursor);
        const std::size_t formStart = input.find(kFormPrefix, cursor);
        const bool isPluginIndex = pluginIndexStart < formStart;
        const std::size_t tokenStart = isPluginIndex ? pluginIndexStart : formStart;

        if (tokenStart == std::string::npos)
        {
            output.append(input, cursor, std::string::npos);
            break;
        }

        output.append(input, cursor, tokenStart - cursor);
        const std::size_t contentStart = tokenStart + (isPluginIndex ? kPluginPrefix.size() : kFormPrefix.size());
        const std::size_t tokenEnd = input.find("}}", contentStart);
        if (tokenEnd == std::string::npos)
            return;

        std::string pluginName;
        std::string localText;
        std::uint32_t local = 0;
        std::size_t trimCount = 0;

        if (isPluginIndex)
        {
            pluginName = input.substr(contentStart, tokenEnd - contentStart);
        }
        else
        {
            const std::size_t pluginEnd = input.find(':', contentStart);
            if (pluginEnd >= tokenEnd)
                return;

            pluginName = input.substr(contentStart, pluginEnd - contentStart);

            const std::size_t localStart = pluginEnd + 1;
            const std::size_t formatStart = input.find(':', localStart);
            const std::size_t localEnd = formatStart < tokenEnd ? formatStart : tokenEnd;

            localText = input.substr(localStart, localEnd - localStart);

            if (formatStart < tokenEnd)
                ParseTrimCount(input.substr(formatStart + 1, tokenEnd - formatStart - 1), trimCount);
            ParseHex(localText, local);
        }

        const LoadedPlugin* plugin = loadOrder.Find(pluginName);
        if (!plugin)
            return;

        if (isPluginIndex)
        {
            if (plugin->isLight)
                output += std::format("FE{:03X}", plugin->lightIndex);
            else
                output += std::format("{:02X}", plugin->fullIndex);
        }
        else
        {
            if (plugin->isLight && localText.size() > 3)
                local >>= static_cast<unsigned int>(((localText.size() < 6 ? localText.size() : 6) - 3) * 4);

            output += ResolveForm(*plugin, local, trimCount);
        }

        cursor = tokenEnd + 2;
    }
}

static void ParseHex(const std::string& text, std::uint32_t& value)
{
    value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value, 16);
}

static void ParseTrimCount(const std::string& format, std::size_t& trimCount)
{
    trimCount = 0;
    if (!format.empty())
        std::from_chars(format.data() + format.size() - 1, format.data() + format.size(), trimCount);
}

static std::string ResolveForm(const LoadedPlugin& plugin, std::uint32_t local, std::size_t trimCount)
{
    std::string form = plugin.isLight ? std::format("FE{:03X}{:03X}", plugin.lightIndex, local) : std::format("{:02X}{:06X}", plugin.fullIndex, local);
    return form.substr(trimCount < form.size() ? trimCount : form.size());
}
