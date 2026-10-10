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
    bool isLight;
    bool isMaster;
    std::vector<std::string> masters;
    PluginHeaderInfo() : isLight(false), isMaster(false) {}
};

struct LoadOrderCandidate
{
    std::string name;
    PluginHeaderInfo header;
};

typedef std::unordered_map<std::string, LoadOrderCandidate> CandidateMap;

struct CandidateLoadContext
{
    const std::filesystem::path& dataDirectory;
    CandidateMap& candidates;
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
    std::size_t i = 0;
    for (i = 0; i < name.size(); ++i)
        name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
    return name;
}

// Reads only the TES4 fields RFR needs
static void ReadPluginHeader(const std::filesystem::path& file, PluginHeaderInfo& header)
{
    static const std::uint32_t kRecordFlagMaster = 0x00000001u;
    static const std::uint32_t kRecordFlagLight = 0x00000200u;

    IFileStream stream;
    std::string path = file.string();
    std::string extension;
    std::int64_t payloadEnd = 0;
    std::uint32_t dataSize = 0;
    std::uint32_t flags = 0;
    std::uint32_t nextSubrecordExtendedSize = 0;

    stream.Open(path.c_str());
    stream.Skip(4);
    dataSize = stream.Read32();
    flags = stream.Read32();
    stream.Skip(12);

    extension = NormalizePluginName(file.extension().string());
    header.isLight = (flags & kRecordFlagLight) != 0 || extension == ".esl";
    header.isMaster = (flags & kRecordFlagMaster) != 0 || extension == ".esm" || extension == ".esl";

    payloadEnd = stream.GetOffset() + static_cast<std::int64_t>(dataSize);

    while (stream.GetOffset() < payloadEnd)
    {
        const std::uint32_t subrecordType = stream.Read32();
        const std::uint16_t shortSize = stream.Read16();
        const std::uint32_t subrecordSize = nextSubrecordExtendedSize != 0 ? nextSubrecordExtendedSize : shortSize;
        nextSubrecordExtendedSize = 0;

        if (subrecordType == CHAR_CODE('X', 'X', 'X', 'X'))
        {
            nextSubrecordExtendedSize = stream.Read32();
            continue;
        }

        if (subrecordType != CHAR_CODE('M', 'A', 'S', 'T'))
        {
            stream.Skip(subrecordSize);
            continue;
        }
        //null-terminated
        std::string master(subrecordSize, '\0');
        stream.ReadBuf(&master[0], subrecordSize);
        master.resize(master.find('\0'));
        header.masters.push_back(master);
    }

}

// Reads either plugins.txt or Skyrim.ccc
static bool ReadPluginList(const std::filesystem::path& path, bool useEnabledMarkers, std::vector<std::string>& entries, std::string& error)
{
    IFileStream stream;
    std::string filePath = path.string();
    bool firstLine = true;
    char buffer[1024] = {};
    char* line = NULL;

    entries.clear();
    error.clear();
    if (!stream.Open(filePath.c_str()))
    {
        error = "cannot open plugin list: " + path.filename().string();
        return false;
    }

    while (!stream.HitEOF())
    {
        buffer[0] = '\0';
        stream.ReadString(buffer, static_cast<std::uint32_t>(sizeof(buffer)), '\n', '\r');

        line = buffer;
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
        entries.push_back(line);
    }

    return true;
}

static void LoadCandidate(const std::string& name, CandidateLoadContext& context)
{
    std::string key = NormalizePluginName(name);
    CandidateMap::iterator found = context.candidates.find(key);
    PluginHeaderInfo header;
    LoadOrderCandidate candidate;

    if (found != context.candidates.end())
        return;

    ReadPluginHeader(context.dataDirectory / name, header);
    candidate.name = name;
    candidate.header = header;
    context.candidates.insert(std::make_pair(key, candidate));

    for (const std::string& masterName : header.masters)
        LoadCandidate(masterName, context);
}

// Emits masters first
static void EmitPlugin(const std::string& name, EmitContext& context)
{
    std::string key = NormalizePluginName(name);
    LoadedPlugin plugin;

    if (context.emitted.find(key) != context.emitted.end())
        return;

    CandidateMap::const_iterator it = context.candidates.find(key);
    const LoadOrderCandidate& candidate = it->second;
    for (const std::string& master : candidate.header.masters)
        EmitPlugin(master, context);

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

// Reconstructs the active full/light load order
bool LoadOrder::Build(const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile, const std::filesystem::path& dataDirectory, std::string& error)
{
    std::vector<std::string> pluginEntries;
    std::vector<std::string> creationEntries;
    std::vector<std::string> masters;
    std::vector<std::string> regular;
    std::unordered_set<std::string> emitted;

    CandidateMap candidates;
    CandidateLoadContext candidateContext = {dataDirectory, candidates};
    std::uint32_t fullIndex = 0;
    std::uint32_t lightIndex = 0;

    EmitContext emitContext = {
        candidates,
        emitted,
        fullIndex,
        lightIndex,
        plugins_,
        byName_ };

    plugins_.clear();
    byName_.clear();

    if (!ReadPluginList(pluginsFile, true, pluginEntries, error))
        return false;

    if (!creationClubFile.empty())
    {
        if (!ReadPluginList(creationClubFile, false, creationEntries, error))
            return false;
        std::erase_if(creationEntries, [&](const std::string& name) { return !std::filesystem::exists(dataDirectory / name); });
    }

    for (const std::string& name : creationEntries)  //load CC first
        LoadCandidate(name, candidateContext);
    for (const std::string& name : pluginEntries)   //then load plugins
        LoadCandidate(name, candidateContext);

    // Preserve high-level order: Creation Club, masters, regular plugins
    enum class PluginGroup
    {
        Master,
        Regular
    };

    for (const std::string& name : creationEntries)
        EmitPlugin(name, emitContext);

    for (const std::string& name : pluginEntries)
    {
        const std::string key = NormalizePluginName(name);
        if (emitted.find(key) != emitted.end())
            continue;

        CandidateMap::const_iterator it = candidates.find(key);
        const PluginGroup group = it->second.header.isMaster ? PluginGroup::Master : PluginGroup::Regular;

        switch (group)
        {
        case PluginGroup::Master:masters.push_back(name);
            break;
        case PluginGroup::Regular:regular.push_back(name);
            break;
        }
    }

    for (const std::string& name : masters)  //emits esm flagged plugin first
        EmitPlugin(name, emitContext);
    for (const std::string& name : regular)
        EmitPlugin(name, emitContext);

    return true;
}

// Finds one indexed plugin
const LoadedPlugin* LoadOrder::Find(const std::string& pluginName) const
{
    std::unordered_map<std::string, std::size_t>::const_iterator it = byName_.find(NormalizePluginName(pluginName));
    if (it != byName_.end())
        return &plugins_[it->second];
    return NULL;
}

bool LoadOrder::Contains(const std::string& pluginName) const
{
    return Find(pluginName) != NULL;
}

std::size_t LoadOrder::Count() const
{
    return plugins_.size();
}
