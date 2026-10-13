#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/IPrefix.h"
#include "common/IFileStream.h"
#include "LoadOrder.h"

struct PluginHeaderInfo
{
    bool isLight = false;
    bool isMaster = false;
    std::vector<std::string> masters;
};

struct LoadOrderCandidate
{
    std::string name;
    PluginHeaderInfo header;
};

using CandidateMap = std::unordered_map<std::string, LoadOrderCandidate>;

struct CandidateLoadContext
{
    const std::filesystem::path& dataDirectory;
    CandidateMap& candidates;
    std::unordered_set<std::string> loading;
};

struct EmitContext
{
    const CandidateMap& candidates;
    std::unordered_set<std::string>& emitted;
    std::uint32_t& fullIndex;
    std::uint32_t& lightIndex;
    std::vector<LoadedPlugin>& plugins;
    std::unordered_map<std::string, std::size_t>& byName;
};

static std::string NormalizePluginName(const std::string& value)
{
    std::string name = value;
    for (char& c : name)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

static bool SetHeaderError(const char* reason, std::string& error)
{
    error = "invalid plugin header: " + std::string(reason);
    return false;
}

static bool ReadPluginHeader(const std::filesystem::path& file, PluginHeaderInfo& header, std::string& error)
{
    constexpr std::uint32_t kRecordFlagMaster = 0x00000001u;
    constexpr std::uint32_t kRecordFlagLight  = 0x00000200u;
    constexpr std::int64_t kRecordHeaderSize = 24;

    header = {};

    IFileStream stream;
    if (!stream.Open(file.string().c_str()))
    {
        error = "cannot open plugin file";
        return false;
    }

    if (stream.GetLength() < kRecordHeaderSize)
        return SetHeaderError("file is too short", error);

    if (stream.Read32() != CHAR_CODE('T', 'E', 'S', '4'))
        return SetHeaderError("unexpected record type", error);

    const std::uint32_t dataSize = stream.Read32();
    const std::uint32_t flags = stream.Read32();
    stream.Skip(12);

    const std::string extension = NormalizePluginName(file.extension().string());
    header.isLight = (flags & kRecordFlagLight) != 0 || extension == ".esl";
    header.isMaster = (flags & kRecordFlagMaster) != 0 || extension == ".esm" || extension == ".esl";

    if (dataSize > static_cast<std::uint64_t>(stream.GetLength() - stream.GetOffset()))
        return SetHeaderError("record payload exceeds file size", error);

    const std::int64_t payloadEnd = stream.GetOffset() + static_cast<std::int64_t>(dataSize);
    bool hasExtendedSize = false;
    std::uint32_t extendedSize = 0;

    while (stream.GetOffset() < payloadEnd)
    {
        if (payloadEnd - stream.GetOffset() < 6)
            return SetHeaderError("truncated subrecord header", error);

        const std::uint32_t subrecordType = stream.Read32();
        const std::uint16_t shortSize = stream.Read16();

        if (subrecordType == CHAR_CODE('X', 'X', 'X', 'X'))
        {
            if (hasExtendedSize || shortSize != sizeof(std::uint32_t) || payloadEnd - stream.GetOffset() < sizeof(std::uint32_t))
                return SetHeaderError("invalid extended subrecord", error);

            extendedSize = stream.Read32();
            hasExtendedSize = true;
            continue;
        }

        const std::uint32_t subrecordSize = hasExtendedSize ? extendedSize : shortSize;
        hasExtendedSize = false;

        if (static_cast<std::uint64_t>(subrecordSize) > static_cast<std::uint64_t>(payloadEnd - stream.GetOffset()))
            return SetHeaderError("subrecord exceeds record payload", error);

        if (subrecordType != CHAR_CODE('M', 'A', 'S', 'T'))
        {
            stream.Skip(subrecordSize);
            continue;
        }

        std::string master(subrecordSize, '\0');
        stream.ReadBuf(master.data(), subrecordSize);
        const std::size_t terminator = master.find('\0');
        if (terminator == std::string::npos || terminator == 0)
            return SetHeaderError("unterminated master name", error);

        master.resize(terminator);
        header.masters.push_back(std::move(master));
    }

    if (hasExtendedSize || stream.GetOffset() != payloadEnd)
        return SetHeaderError("incomplete record payload", error);

    return true;
}

static bool ReadPluginList(const std::filesystem::path& path, bool useEnabledMarkers, std::vector<std::string>& entries, std::string& error)
{
    entries.clear();
    error.clear();

    IFileStream stream;
    if (!stream.Open(path.string().c_str()))
    {
        error = "cannot open plugin list: " + path.filename().string();
        return false;
    }

    bool firstLine = true;
    char buffer[1024]{};

    while (!stream.HitEOF())
    {
        buffer[0] = '\0';
        stream.ReadString(buffer, static_cast<std::uint32_t>(sizeof(buffer)), '\n', '\r');

        char* line = buffer;
        if (firstLine && static_cast<std::uint8_t>(line[0]) == 0xEF && static_cast<std::uint8_t>(line[1]) == 0xBB && static_cast<std::uint8_t>(line[2]) == 0xBF)
            line += 3;
        firstLine = false;

        if (!line[0] || line[0] == '#')
            continue;

        if (useEnabledMarkers)
        {
            if (line[0] != '*')
                continue;
            ++line;
        }
        entries.emplace_back(line);
    }

    return true;
}

static bool LoadCandidate(const std::string& name, const std::string& requiredBy, CandidateLoadContext& context, std::string& error)
{
    const std::string key = NormalizePluginName(name);

    if (context.loading.contains(key))
    {
        error = "cyclic master dependency involving '" + name + "'";
        return false;
    }

    if (context.candidates.contains(key))
        return true;

    context.loading.insert(key);

    const std::filesystem::path file = context.dataDirectory / name;
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
    {
        if (ec)
            error = "cannot access plugin '" + name + "': " + ec.message();
        else if (requiredBy.empty())
            error = "plugin not found: " + name;
        else
            error = "missing master '" + name + "' required by '" + requiredBy + "'";
        return false;
    }

    PluginHeaderInfo header;
    if (!ReadPluginHeader(file, header, error))
    {
        if (requiredBy.empty())
            error = "invalid plugin '" + name + "': " + error;
        else
            error = "invalid master '" + name + "' required by '" + requiredBy + "': " + error;
        return false;
    }

    LoadOrderCandidate candidate{ name, header };
    context.candidates.emplace(key, std::move(candidate));

    for (const std::string& masterName : header.masters)
    {
        if (!LoadCandidate(masterName, name, context, error))
            return false;
    }

    context.loading.erase(key);
    return true;
}

static void EmitPlugin(const std::string& name, EmitContext& context)
{
    const std::string key = NormalizePluginName(name);
    if (context.emitted.contains(key))
        return;

    const auto& candidate = context.candidates.at(key);
    for (const std::string& master : candidate.header.masters)
        EmitPlugin(master, context);

    LoadedPlugin plugin;
    plugin.name = candidate.name;
    plugin.isLight = candidate.header.isLight;

    if (plugin.isLight)
        plugin.lightIndex = static_cast<std::uint16_t>(context.lightIndex++);
    else
        plugin.fullIndex = static_cast<std::uint8_t>(context.fullIndex++);

    context.byName[key] = context.plugins.size();
    context.plugins.push_back(plugin);
    context.emitted.insert(key);
}

bool LoadOrder::Build(const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile, const std::filesystem::path& dataDirectory, std::string& error)
{
    plugins_.clear();
    byName_.clear();
    explicitlyEnabled_.clear();

    std::vector<std::string> pluginEntries;
    if (!ReadPluginList(pluginsFile, true, pluginEntries, error))
        return false;

    for (const std::string& name : pluginEntries)
        explicitlyEnabled_.insert(NormalizePluginName(name));

    std::vector<std::string> creationEntries;
    if (!creationClubFile.empty())
    {
        if (!ReadPluginList(creationClubFile, false, creationEntries, error))
            return false;
        std::erase_if(creationEntries, [&](const std::string& name) { return !std::filesystem::exists(dataDirectory / name); });
    }

    CandidateMap candidates;
    CandidateLoadContext candidateContext{ dataDirectory, candidates };

    for (const std::string& name : creationEntries)
    {
        if (!LoadCandidate(name, {}, candidateContext, error))
            return false;
    }

    for (const std::string& name : pluginEntries)
    {
        if (!LoadCandidate(name, {}, candidateContext, error))
            return false;
    }

    std::uint32_t fullIndex = 0;
    std::uint32_t lightIndex = 0;
    std::unordered_set<std::string> emitted;

    EmitContext emitContext{
        candidates,
        emitted,
        fullIndex,
        lightIndex,
        plugins_,
        byName_
    };

    for (const std::string& name : creationEntries)
        EmitPlugin(name, emitContext);

    std::vector<std::string> masters;
    std::vector<std::string> regular;

    for (const std::string& name : pluginEntries)
    {
        const std::string key = NormalizePluginName(name);
        if (emitted.contains(key))
            continue;

        const auto& candidate = candidates.at(key);
        if (candidate.header.isMaster)
            masters.push_back(name);
        else
            regular.push_back(name);
    }

    for (const std::string& name : masters)
        EmitPlugin(name, emitContext);
    for (const std::string& name : regular)
        EmitPlugin(name, emitContext);

    return true;
}

const LoadedPlugin* LoadOrder::Find(const std::string& pluginName) const
{
    auto it = byName_.find(NormalizePluginName(pluginName));
    if (it != byName_.end())
        return &plugins_[it->second];
    return nullptr;
}

bool LoadOrder::IsExplicitlyEnabled(const std::string& pluginName) const
{
    return explicitlyEnabled_.contains(NormalizePluginName(pluginName));
}

std::size_t LoadOrder::Count() const
{
    return plugins_.size();
}
