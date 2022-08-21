/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022 Filip Szczerek <ga.software@yahoo.com>

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
    Script worker thread header.
*/

#pragma once

#include <istream>
#include <future>
#include <memory>
#include <wx/event.h>
#include <wx/string.h>
#include <wx/thread.h>

namespace scripting
{

class ScriptRunner: public wxThread
{
public:
    ScriptRunner(std::unique_ptr<std::istream> script, wxEvtHandler& parent, std::future<void>&& stopRequested);
    ~ScriptRunner();

private:
    ExitCode Entry() override;

    std::unique_ptr<std::istream> m_Script;
    wxEvtHandler& m_Parent;
    std::future<void> m_StopRequested;
};

}
