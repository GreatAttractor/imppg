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
    Directory iterator header.
*/

#pragma once

#include <wx/dir.h>
#include <wx/filename.h>

#include <filesystem>
#include <lua.hpp>
#include <memory>
#include <optional>
#include <string>

namespace scripting
{

class DirectoryIterator
{
public:
    static DirectoryIterator Create(wxString filePathPattern);

    std::optional<wxFileName> Next();

private:
    DirectoryIterator(wxString filePathPattern);

    std::unique_ptr<wxDir> m_DirIter;

    wxFileName m_Dir;

    std::optional<wxFileName> m_FirstResult;
};

}
