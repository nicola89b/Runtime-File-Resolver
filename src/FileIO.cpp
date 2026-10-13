#include <cstdint>
#include "common/IPrefix.h"
#include "common/IFileStream.h"

#include "FileIO.h"

namespace RuntimeFileResolver
{
    bool ReadTextFile(const std::filesystem::path& path, std::string& text, std::string* outError)
    {
        text.clear();
        if (outError)
            outError->clear();

        IFileStream stream;
        const std::string filePath = path.string();

        if (!stream.Open(filePath.c_str()))
        {
            if (outError)
                *outError = "cannot open file for reading: " + path.filename().string();
            return false;
        }

        const auto length = stream.GetLength();
        text.resize(static_cast<std::size_t>(length));

        if (!text.empty())
            stream.ReadBuf(&text[0], static_cast<std::uint32_t>(text.size()));

        return true;
    }

    bool WriteTextFile(const std::filesystem::path& path, std::string_view text, std::string* outError)
    {
        if (outError)
            outError->clear();

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
        {
            if (outError)
                *outError = "cannot create target directory (" + ec.message() + ")";
            return false;
        }

        IFileStream stream;
        const std::string filePath = path.string();
        if (!stream.Create(filePath.c_str()))
        {
            if (outError)
                *outError = "cannot open target file for writing";
            return false;
        }

        if (!text.empty())
            stream.WriteBuf(text.data(), static_cast<std::uint32_t>(text.size()));

        if (stream.GetOffset() != static_cast<std::int64_t>(text.size()))
        {
            stream.Close();
            if (outError)
                *outError = "failed while writing target file";
            return false;
        }

        stream.Close();
        return true;
    }
}
