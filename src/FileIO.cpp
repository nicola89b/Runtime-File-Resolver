#include <cstdint>
#include "common/IPrefix.h"
#include "common/IFileStream.h"

#include "FileIO.h"

// Reads the template file
void ReadTextFile(const std::filesystem::path& path, std::string& text)
{
    IFileStream stream;
    std::string filePath = path.string();
    std::int64_t length = 0;

    text.clear();
    if (!stream.Open(filePath.c_str()))
        return;

    length = stream.GetLength();
    text.resize(length);
    if (!text.empty())
        stream.ReadBuf(&text[0], static_cast<std::uint32_t>(text.size()));
}

// Writes the target file
bool WriteTextFile(const std::filesystem::path& path, const std::string& text, std::string& error)
{
    std::error_code ec;
    IFileStream stream;
    std::string filePath;

    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        error = "cannot create target directory (" + ec.message() + ")";
        return false;
    }

    filePath = path.string();
    if (!stream.Create(filePath.c_str()))
    {
        error = "cannot open target file for writing";
        return false;
    }

    if (!text.empty())
        stream.WriteBuf(text.data(), static_cast<std::uint32_t>(text.size()));

    if (stream.GetOffset() != static_cast<std::int64_t>(text.size()))
    {
        stream.Close();
        error = "failed while writing target file";
        return false;
    }
    stream.Close();

    return true;
}
