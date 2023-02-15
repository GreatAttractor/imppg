#include "interop/classes/DirectoryIterator.h"
#include "scripting/script_exceptions.h"

#include <wx/intl.h>

namespace fs = std::filesystem;

// private definitions
namespace
{

std::string ConvertFileNamePatternToRegEx(std::string_view input)
{
    static const std::string charsToEscape{"[\\^$.|?+(){}"};

    std::string result;
    for (const char c: input)
    {
        if ('*' == c)
        {
            result += ".*";
        }
        else if (charsToEscape.find(c) != std::string_view::npos)
        {
            result = result + "\\" + c;
        }
        else
        {
            result += c;
        }
    }

    return result;
}

}

namespace scripting
{

DirectoryIterator::DirectoryIterator(std::string fileNamePattern)
{
    const auto patternPath = fs::path{fileNamePattern};

    auto dirToIterateIn = patternPath;

    if (fs::status(patternPath).type() == fs::file_type::directory)
    {
        fileNamePattern = (patternPath / "*").generic_string<char>();
    }
    else
    {
        dirToIterateIn = patternPath.parent_path();
    }
    m_RegEx = std::regex{ConvertFileNamePatternToRegEx(fileNamePattern)};
    std::error_code ec{};
    m_DirIter = std::filesystem::directory_iterator{dirToIterateIn, ec};
    if (ec)
    {
        throw ScriptExecutionError{wxString::Format(_("cannot access directory %s"), dirToIterateIn.generic_string())};
    }
}

std::optional<std::string> DirectoryIterator::Next()
{
    while (m_DirIter != std::filesystem::end(m_DirIter))
    {
        const std::string item = m_DirIter->path().generic_string<char>();
        ++m_DirIter;
        if (std::regex_match(item, m_RegEx))
        {
            return item;
        }
    }

    return std::nullopt;
}

}
