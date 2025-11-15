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

#include "common/common.h"
#include "interop/classes/DirectoryIterator.h"
#include "scripting/script_exceptions.h"

#include <wx/filename.h>
#include <wx/intl.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace scripting
{

DirectoryIterator DirectoryIterator::Create(wxString filePathPattern)
{
    return DirectoryIterator(filePathPattern);
}

DirectoryIterator::DirectoryIterator(wxString filePathPattern)
: m_DirIter{std::make_unique<wxDir>()}
{
    const auto patternPath = wxFileName{filePathPattern};

    const bool isDirectory = (fs::status(ToFsPath(patternPath.GetFullPath())).type() == fs::file_type::directory);

    auto fileNamePattern = patternPath.GetFullName();

    if (isDirectory)
    {
        fileNamePattern = "*";
        m_Dir.SetPath(filePathPattern);
    }
    else
    {
        m_Dir.SetPath(wxFileName{filePathPattern}.GetPath());
    }

    if (!m_DirIter->Open(m_Dir.GetPath()))
    {
        throw ScriptExecutionError{wxString::Format(_("cannot access directory %s"), m_Dir.GetPath())};
    }

    wxString f;
    if (m_DirIter->GetFirst(&f, fileNamePattern, wxDIR_FILES))
    {
        wxFileName fn{m_Dir};
        fn.SetFullName(f);
        m_FirstResult = fn;
    }
}

std::optional<wxFileName> DirectoryIterator::Next()
{
    if (m_FirstResult.has_value())
    {
        const auto result = std::move(m_FirstResult.value());
        m_FirstResult = std::nullopt;
        return result;
    }
    else
    {
        wxString f;
        if (m_DirIter->GetNext(&f))
        {
            wxFileName fn{m_Dir};
            fn.SetFullName(f);
            return fn;
        }
        else
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

}
