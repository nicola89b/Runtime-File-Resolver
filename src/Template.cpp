#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>

#include "Template.h"
#include "LoadOrder.h"

static void ParseHex(const std::string& text, std::uint32_t& value);
static void ParseTrimCount(const std::string& format, std::size_t& trimCount);
static std::string ResolveForm(const LoadedPlugin& plugin, std::uint32_t local, std::size_t trimCount);

// Scan and replace
void ResolveTemplate(const std::string& input, const LoadOrder& loadOrder, std::string& output)
{
    static const char kPluginIndexPrefix[] = "{{PLUGIN_INDEX:";
    static const char kFormPrefix[] = "{{FORM:";
    static const std::size_t kPluginIndexPrefixLength = sizeof(kPluginIndexPrefix) - 1;
    static const std::size_t kFormPrefixLength = sizeof(kFormPrefix) - 1;
    const std::string& source = input;
    std::size_t cursor = 0;
    output.clear();
    output.reserve(source.size() + 64);

    for (;;)
    {
        const std::size_t pluginIndexStart = source.find(kPluginIndexPrefix, cursor);
        const std::size_t formStart = source.find(kFormPrefix, cursor);
        const bool isPluginIndex = pluginIndexStart < formStart;
        const std::size_t tokenStart = isPluginIndex ? pluginIndexStart : formStart;
        std::size_t contentStart = 0;
        std::size_t tokenEnd = 0;
        std::size_t pluginEnd = 0;
        std::size_t localStart = 0;
        std::size_t localEnd = 0;
        std::size_t formatStart = std::string::npos;
        std::uint32_t local = 0;
        std::string pluginName;
        std::string localText;
        std::size_t trimCount = 0;

        if (tokenStart == std::string::npos)
        {
            output.append(source, cursor, std::string::npos);
            break;
        }

        //  Token validation
        output.append(source, cursor, tokenStart - cursor);
        contentStart = tokenStart + (isPluginIndex ? kPluginIndexPrefixLength : kFormPrefixLength);
        tokenEnd = source.find("}}", contentStart);
        if (tokenEnd == std::string::npos)
            return;

        if (isPluginIndex)
        {
            pluginName = source.substr(contentStart, tokenEnd - contentStart);
        }
        else
        {
            pluginEnd = source.find(':', contentStart);
            if (pluginEnd >= tokenEnd)
                return;

            pluginName = source.substr(contentStart, pluginEnd - contentStart);

            localStart = pluginEnd + 1;
            formatStart = source.find(':', localStart);
            localEnd = formatStart < tokenEnd ? formatStart : tokenEnd;
            localText = source.substr(localStart, localEnd - localStart);

            if (formatStart < tokenEnd)
                ParseTrimCount(source.substr(formatStart + 1, tokenEnd - formatStart - 1), trimCount);
            ParseHex(localText, local);
        }

        const LoadedPlugin* plugin = loadOrder.Find(pluginName);
        if (plugin == NULL)
            return;


        // Token replacement
        if (isPluginIndex){
            if (plugin->isLight)
                output += std::format("FE{:03X}", plugin->lightIndex);
            else
                output += std::format("{:02X}", plugin->fullIndex);
        }
        else{
            if (plugin->isLight && localText.size() > 3)
                local >>= static_cast<unsigned int>(((localText.size() < 6 ? localText.size() : 6) - 3) * 4);

            output += ResolveForm(*plugin, local, trimCount);
        }

        cursor = tokenEnd + 2;
    }

}

// Helpers
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
    std::string form;
    if (plugin.isLight)
        form = std::format("FE{:03X}{:03X}", plugin.lightIndex, local);
    else
        form = std::format("{:02X}{:06X}", plugin.fullIndex, local);

    return form.substr(trimCount < form.size() ? trimCount : form.size());
}
