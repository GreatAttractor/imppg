/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2017 Filip Szczerek <ga.software@yahoo.com>

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
    Logging functions header.
*/

#ifndef IMPPG_LOGGING_H
#define IMPPG_LOGGING_H

#include <iostream>
#include <wx/string.h>

namespace Log
{

typedef enum { QUIET = 0, NORMAL, VERBOSE} LogLevel_t;

void Initialize(LogLevel_t level, std::ostream &outputStream);

/// Prints a message. Newline is NOT added by default.
void Print(const wxString &msg, bool prependTimestamp = true, LogLevel_t level = NORMAL);

}


#endif
