/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2023-2025 Filip Szczerek <ga.software@yahoo.com>

This file is part of ImPPG.

ImPPG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ImPPG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ImPPG.  If not, see <http://www.gnu.org/licenses/>.

File description:
    Directory iterator implementation.
*/

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

DirectoryIterator DirectoryIterator::Create(std::string fileNamePattern)
{
    return DirectoryIterator(fileNamePattern);
}

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
