/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2024 Filip Szczerek <ga.software@yahoo.com>

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
    Common directories: implementation.
*/

#include "common/dirs.h"

#include <filesystem>
#include <wx/stdpaths.h>

namespace fs = std::filesystem;

wxFileName GetImagesDirectory()
{
    auto imgDir = wxFileName(wxStandardPaths::Get().GetExecutablePath());
    imgDir.AppendDir("images");

    if (!imgDir.Exists())
    {
        imgDir.AssignCwd();
        imgDir.AppendDir("images");
        if (!imgDir.Exists())
        {
            imgDir.AssignDir(IMPPG_IMAGES_DIR); // defined in CMakeLists.txt
        }
    }

    return imgDir;
}

std::filesystem::path GetUserDataDir()
{
#if defined __WXMSW__
    if (const auto* env = std::getenv("APPDATA"))
    {
        return env;
    }
    else
    {
        return {};
    }
#elif defined __APPLE__
    if (const auto* env = std::getenv("HOME"))
    {
        return fs::path{env} / "Library" / "Application Support";
    }
    else
    {
        return {};
    }
#else
    if (const auto* env = std::getenv("XDG_DATA_HOME"))
    {
        return fs::path{env};
    }
    else if (const auto* env = std::getenv("HOME"))
    {
        return fs::path{env} / ".local" / "share";
    }
    else
    {
        return {};
    }
#endif
}
