#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct LoadedPlugin
{
    std::string name;
    bool isLight;
    std::uint8_t fullIndex;
    std::uint16_t lightIndex;
    LoadedPlugin() : isLight(false), fullIndex(0), lightIndex(0) {}
};


class LoadOrder
{
  public:
    bool Build(const std::filesystem::path& pluginsFile, const std::filesystem::path& creationClubFile, const std::filesystem::path& dataDirectory, std::string& error);
    const LoadedPlugin* Find(const std::string& pluginName) const;
    bool Contains(const std::string& pluginName) const;
    std::size_t Count() const;

  private:
    std::vector<LoadedPlugin> plugins_;
    std::unordered_map<std::string, std::size_t> byName_;
};
