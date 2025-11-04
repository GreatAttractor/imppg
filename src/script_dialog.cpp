/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Script dialog implementation.
*/

#include "appconfig.h"
#include "common/common.h"
#include "common/proc_settings.h"
#include "ctrl_ids.h"
#include "common/imppg_assert.h"
#include "progress_bar.h"
#include "scripting/interop.h"
#include "script_dialog.h"

#include <fstream>
#include <wx/button.h>
#include <wx/filename.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

namespace scripting
{

// private definitions
namespace
{

constexpr int BORDER = 5; ///< Border size (in pixels) between controls.

constexpr int PROGRESS_PULSE_INTERVAL_MS = 250;

}

c_ScriptDialog::c_ScriptDialog(wxWindow* parent)
: c_ScrollableDialog(parent, wxID_ANY, _("Run script"))
{
    Bind(wxEVT_CLOSE_WINDOW, &c_ScriptDialog::OnClose, this);
    Bind(wxEVT_IDLE, &c_ScriptDialog::OnIdle, this);
    Bind(wxEVT_THREAD, &c_ScriptDialog::OnRunnerMessage, this);

    InitControls(BORDER);

    wxRect r = Configuration::ScriptDialogPosSize;
    SetPosition(r.GetPosition());
    SetSize(r.GetSize());
    FixWindowPosition(*this);

    const bool nfv = Configuration::NormalizeFITSValues;
    switch (Configuration::ProcessingBackEnd)
    {
    case BackEnd::CPU_AND_BITMAPS:
        m_Processor = std::make_unique<scripting::ScriptImageProcessor>(
            imppg::backend::CreateCpuBmpProcessingBackend(),
            nfv
        );
        break;

#if USE_OPENGL_BACKEND
    case BackEnd::GPU_OPENGL:
        m_Processor = std::make_unique<scripting::ScriptImageProcessor>(
            imppg::backend::CreateOpenGLProcessingBackend(Configuration::LRCmdBatchSizeMpixIters),
            nfv
        );
        break;
#endif
    default: IMPPG_ABORT();
    }
}

void c_ScriptDialog::DoInitControls()
{
    wxSizer* szTop = new wxBoxSizer(wxVERTICAL);

    wxSizer* szScriptFile = new wxBoxSizer(wxHORIZONTAL);

    szScriptFile->Add(
        new wxStaticText(GetContainer(),
        wxID_ANY,
        _("Script file:")),
        0,
        wxALIGN_CENTER_VERTICAL | wxALL,
        BORDER
    );

    m_ScriptFileCtrl = new wxFilePickerCtrl(
        GetContainer(),
        wxID_ANY,
        "",
        _("Choose the script file to run"),
        _("LUA files (*.lua)") + "|*.lua|*.*|*.*",
        wxDefaultPosition,
        wxDefaultSize,
        wxFD_OPEN | wxFD_FILE_MUST_EXIST
    );
    m_ScriptFileCtrl->Bind(wxEVT_FILEPICKER_CHANGED, &c_ScriptDialog::OnScriptFileSelected, this);
    szScriptFile->Add(m_ScriptFileCtrl, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    m_ScriptFileCtrl->SetInitialDirectory(Configuration::ScriptOpenPath);
    m_ScriptFilePath =
        new wxStaticText(GetContainer(), wxID_ANY, _("Select the script file."), wxDefaultPosition, wxDefaultSize);
    szScriptFile->Add(m_ScriptFilePath, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    szTop->Add(szScriptFile, 0, wxALIGN_LEFT | wxALL, BORDER);

    wxSizer* szRunControls = new wxBoxSizer(wxHORIZONTAL);

    m_BtnRun = new wxButton(GetContainer(), wxID_ANY, _("Run"));
    m_BtnRun->Bind(wxEVT_BUTTON, &c_ScriptDialog::OnRunScript, this);
    m_BtnRun->Disable();
    szRunControls->Add(m_BtnRun, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    m_BtnStop = new wxButton(GetContainer(), wxID_ANY, _("Stop"));
    m_BtnStop->Bind(wxEVT_BUTTON, &c_ScriptDialog::OnStopScript, this);
    m_BtnStop->Disable();
    szRunControls->Add(m_BtnStop, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    m_BtnTogglePause = new wxButton(GetContainer(), wxID_ANY, _("Pause"));
    m_BtnTogglePause->Bind(wxEVT_BUTTON, &c_ScriptDialog::OnTogglePause, this);
    m_BtnTogglePause->Disable();
    szRunControls->Add(m_BtnTogglePause, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    szTop->Add(szRunControls, 0, wxALIGN_LEFT | wxALL, BORDER);

    m_Progress = new c_ProgressBar(GetContainer(), 100);
    m_Progress->SetMinClientSize({-1, static_cast<int>(1.5 * m_Progress->GetTextExtent("M").GetHeight())});
    szTop->Add(m_Progress, 0, wxEXPAND, BORDER);

    m_ProgressTimer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { m_Progress->Pulse(); });

    m_Console = new wxRichTextCtrl(GetContainer());
    m_Console->SetEditable(false);
    m_Console->SetMinSize(wxSize(0, 2 * m_Console->GetTextExtent("M").y));
    szTop->Add(m_Console, 1, wxEXPAND, BORDER);

    AssignContainerSizer(szTop);

    GetTopSizer()->Add(new wxStaticLine(this), 0, wxGROW | wxALL, BORDER);
    wxSizer* szButtons = new wxBoxSizer(wxHORIZONTAL);
    wxButton* btnClose = new wxButton(this, wxID_CLOSE);
    btnClose->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
    szButtons->Add(btnClose, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    GetTopSizer()->Add(szButtons, 0, wxALIGN_RIGHT | wxALL, BORDER);
}

void c_ScriptDialog::OnScriptFileSelected(wxFileDirPickerEvent& event)
{
    IMPPG_ASSERT(!IsRunnerActive());
    m_ScriptFilePath->SetLabel(event.GetPath());
    m_BtnRun->Enable();
    Configuration::ScriptOpenPath = wxFileName(event.GetPath()).GetPath();
    m_Progress->SetValue(0);
}

void c_ScriptDialog::OnRunScript(wxCommandEvent&)
{
    IMPPG_ASSERT(!IsRunnerActive());

    const auto scriptPath = m_ScriptFileCtrl->GetPath();

    m_Console->AppendText(wxString::Format(_("Running script %s..."), scriptPath) + "\n");

    auto scriptStream = std::make_unique<std::ifstream>(ToFsPath(scriptPath), std::ios::binary);
    if (!scriptStream->is_open())
    {
        wxMessageBox(_("Failed to open script file."), _("Error"), wxICON_ERROR, this);
        return;
    }
    m_StopScript = std::make_unique<std::promise<void>>();
    m_Runner = std::make_unique<ScriptRunner>(std::move(scriptStream), *this, m_StopScript->get_future());
    m_Runner->Run();
    m_BtnRun->Disable();
    m_BtnStop->Enable();
    // TODO: implement script pausing m_BtnTogglePause->Enable();
    m_ScriptFileCtrl->Disable();
    m_Progress->Pulse();
    m_ProgressTimer.Start(PROGRESS_PULSE_INTERVAL_MS);
}

bool c_ScriptDialog::IsRunnerActive() const
{
    return m_Runner && m_Runner->IsRunning();
}

void c_ScriptDialog::OnTogglePause(wxCommandEvent&)
{
    IMPPG_ASSERT_MSG(false, "Not yet implemented.");
}

void c_ScriptDialog::OnStopScript(wxCommandEvent&)
{
    m_BtnStop->Disable();
    m_BtnTogglePause->Disable();
    try { m_StopScript->set_value(); } catch (...) {}
    m_Console->AppendText(_("Waiting for the script to stop...") + "\n");
}

void c_ScriptDialog::OnClose(wxCloseEvent& event)
{
    if (IsRunnerActive() && event.CanVeto())
    {
        if (!m_CloseAfterRunnerEnds)
        {
            if (wxMessageBox(
                _("The script is still running. Do you want to stop it?"),
                _("Information"),
                wxICON_QUESTION | wxYES_NO
            ) == wxYES)
            {
                try { m_StopScript->set_value(); } catch (...) {}
                m_CloseAfterRunnerEnds = true;
                m_BtnStop->Disable();
            }
        }
        else
        {
            wxMessageBox(
                _("The window will be closed after the script ends."),
                _("Information"),
                wxICON_INFORMATION | wxOK
            );
        }

        event.Veto();
    }
    else
    {
        event.Skip();
    }

    Configuration::ScriptDialogPosSize = wxRect(GetPosition(), GetSize());
}

void c_ScriptDialog::OnIdle(wxIdleEvent& event)
{
    if (!IsRunnerActive() && m_CloseAfterRunnerEnds)
    {
        Close();
    }

    m_Processor->OnIdle(event);
}

void c_ScriptDialog::OnRunnerMessage(wxThreadEvent& event)
{
    auto payload = event.GetPayload<ScriptMessagePayload>();

    const auto handler = Overload{
        [&](const contents::None&) {},

        [&](const contents::Error& contents) {
            m_Console->AppendText(_("Script execution error:") + " " + contents.message + ".\n");
            m_Processor->AbortProcessing();
        },

        [&](const contents::Warning& contents) {
            m_Console->AppendText(_("Warning:") + " " + contents.message + ".\n");
        },

        [&](const contents::ScriptFinished&) {
            m_Console->AppendText(_("Script execution finished.") + "\n");
            m_Runner->Wait();
            m_BtnRun->Enable();
            m_BtnStop->Disable();
            m_BtnTogglePause->Disable();
            m_ScriptFileCtrl->Enable();
            m_Progress->SetValue(100);
            m_ProgressTimer.Stop();
        },

        [&](const contents::Progress& contents) {
            m_ProgressTimer.Stop();
            m_Progress->SetValue(100 * contents.fraction);
        },

        [&](const contents::PrintMessage& contents) {
            m_Console->AppendText(contents.message + "\n");
        },

        [&](const auto& contents) {
            // We need to make copies first, because in `StartProcessing` invocation we also move from `payload`,
            // and function argument evaluation order is unspecified.
            scripting::MessageContents contentsCopy = contents;
            auto heartbeat = payload.GetHeartbeat();
            m_Processor->StartProcessing(
                contentsCopy,
                heartbeat,
                [payload = std::move(payload)](scripting::FunctionCallResult result) mutable {
                    payload.SignalCompletion(std::move(result));
                }
            );
            // Some processors may rely on idle events, e.g., the OpenGL back end for driving L-R deconvolution.
            QueueEvent(new wxIdleEvent{});
        }
    };

    std::visit(handler, payload.GetContents());
}

}
