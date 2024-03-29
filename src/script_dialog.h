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
    Script dialog header.
*/

#pragma once

#include "backend/backend.h"
#include "scripting/script_image_processor.h"
#include "scripting/script_runner.h"
#include "scrollable_dlg.h"

#include <future>
#include <memory>

#include <wx/dialog.h>
#include <wx/timer.h>

class c_ProgressBar;
class wxButton;
class wxFilePickerCtrl;
class wxFileDirPickerEvent;
class wxRichTextCtrl;
class wxStaticText;

namespace scripting
{

class c_ScriptDialog: public c_ScrollableDialog
{
public:
    c_ScriptDialog(wxWindow* parent);

private:
    void DoInitControls() override;
    bool IsRunnerActive() const;

    void OnClose(wxCloseEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnRunScript(wxCommandEvent&);
    void OnScriptFileSelected(wxFileDirPickerEvent& event);
    void OnScriptMessageContents(wxThreadEvent& event);
    void OnStopScript(wxCommandEvent&);
    void OnRunnerMessage(wxThreadEvent& event);
    void OnTogglePause(wxCommandEvent&);

    wxFilePickerCtrl* m_ScriptFileCtrl{nullptr};
    wxStaticText* m_ScriptFilePath{nullptr};
    wxButton* m_BtnRun{nullptr};
    wxButton* m_BtnStop{nullptr};
    wxButton* m_BtnTogglePause{nullptr};
    std::unique_ptr<ScriptRunner> m_Runner;
    wxRichTextCtrl* m_Console{nullptr};
    c_ProgressBar* m_Progress{nullptr};
    wxTimer m_ProgressTimer;

    bool m_CloseAfterRunnerEnds{false};
    std::unique_ptr<std::promise<void>> m_StopScript;
    std::unique_ptr<scripting::ScriptImageProcessor> m_Processor;
};

}
