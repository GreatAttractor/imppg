/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2019 Filip Szczerek <ga.software@yahoo.com>

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
    Logging functions implementation.
*/

#include <wx/thread.h>
#include <wx/datetime.h>

#include "logging.h"

std::ostream* logStream = nullptr;
auto logLevel = Log::LogLevel::QUIET;
wxMutex logMutex;

void Log::Initialize(LogLevel level, std::ostream& outputStream)
{
    logStream = &outputStream;
    logLevel = level;
}

void Log::Print(const wxString& msg, bool prependTimestamp, LogLevel level)
{
    if (level <= logLevel)
    {
        wxMutexLocker lock(logMutex);
        //TODO: use something more precise for time source (microsecond precision or better)
        if (prependTimestamp)
        {
            *logStream << wxDateTime::UNow().Format("%H:%M:%S.%l").c_str() << " ";
        }
        *logStream << msg.c_str();
        logStream->flush();
    }
}
