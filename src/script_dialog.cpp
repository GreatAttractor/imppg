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
    Script dialog implementation.
*/

#include "appconfig.h"
#include "common/common.h"
#include "ctrl_ids.h"
#include "imppg_assert.h"
#include "scripting/interop.h"
#include "script_dialog.h"
#include "settings.h"

#include <chrono> //TESTING ##########
#include <fstream>
#include <iostream> //TESTING ###########
#include <thread> //TESTING ###########
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>

using namespace std::chrono_literals; // TESTING ############

namespace scripting
{

// private definitions
namespace
{

constexpr int BORDER = 5; ///< Border size (in pixels) between controls.

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

    //TODO:
//     switch (Configuration::ProcessingBackEnd)
//     {
//     case BackEnd::CPU_AND_BITMAPS: m_Processor = imppg::backend::CreateCpuBmpProcessingBackend(); break;
// #if USE_OPENGL_BACKEND
//     case BackEnd::GPU_OPENGL: m_Processor = imppg::backend::CreateOpenGLProcessingBackend(Configuration::LRCmdBatchSizeMpixIters); break;
// #endif
//     default: IMPPG_ABORT();
//     }

    m_Processor = imppg::backend::CreateCpuBmpProcessingBackend(); //TESTING #######
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

    m_Console = new wxRichTextCtrl(GetContainer());
    m_Console->SetEditable(false);
    m_Console->SetScale(GetContentScaleFactor(), true); //FIXME: text still too small on Linux/GTK3 with 185% global scale
    m_Console->SetMinSize(wxSize(0, 2 * m_Console->GetTextExtent("M").y));
    szTop->Add(m_Console, 1, wxALIGN_CENTER | wxEXPAND, BORDER);

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
}

void c_ScriptDialog::OnRunScript(wxCommandEvent&)
{
    IMPPG_ASSERT(!IsRunnerActive());

    auto scriptStream = std::make_unique<std::ifstream>(m_ScriptFileCtrl->GetPath(), std::ios::binary);
    m_StopScript = std::make_unique<std::promise<void>>();
    m_Runner = std::make_unique<ScriptRunner>(std::move(scriptStream), *this, m_StopScript->get_future());
    m_Runner->Run();
    m_BtnRun->Disable();
    m_BtnStop->Enable();
    m_BtnTogglePause->Enable();
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

    if (m_Processor) { m_Processor->OnIdle(event); }
}

void c_ScriptDialog::OnRunnerMessage(wxThreadEvent& event)
{
    switch (event.GetId())
    {
    case scripting::MessageId::ScriptError:
    {
        auto payload = event.GetPayload<ScriptMessagePayload>();
        m_Console->AppendText(_("Script execution error: ") + payload.GetMessage() + ".\n");
        break;
    }

    case scripting::MessageId::ScriptFinished:
        m_Console->AppendText(_("Script execution finished.") + "\n");
        m_Runner->Wait();
        m_BtnRun->Enable();
        m_BtnStop->Disable();
        m_BtnTogglePause->Disable();
        break;

    case scripting::MessageId::ScriptFunctionCall:
        OnScriptFunctionCall(event);
        break;

    default: break;
    }
}

void c_ScriptDialog::OnScriptFunctionCall(wxThreadEvent& event)
{
    auto payload = event.GetPayload<ScriptMessagePayload>();

    const auto& call = payload.GetCall();
    if (std::get_if<call::Dummy>(&call))
    {
        std::cout << "Main thread: simulating long operation..." << std::endl;
        std::this_thread::sleep_for(1s);
        payload.SignalCompletion(call_result::Success{});
    }
    else if (const auto* processImageFile = std::get_if<call::ProcessImageFile>(&call))
    {
        //temporary experimental code for RGB processing
        const c_Image image = [processImageFile]() {
            auto result = LoadImageAs(processImageFile->imagePath, "tif", PixelFormat::PIX_RGB32F, nullptr, false);
            if (!result) { throw std::runtime_error("failed to load image"); }
            return *result;
        }();

        ProcessingSettings settings;
        IMPPG_ASSERT(LoadSettings(processImageFile->settingsPath, settings, nullptr, nullptr, nullptr));

        const bool isRGB = (NumChannels[static_cast<std::size_t>(image.GetPixelFormat())] == 3);
        if (!isRGB) { throw std::runtime_error("not yet supported"); }

        auto input = image.SplitRGB();

        //FIXME: add proper OnIdle interaction; for now just a blocking approach (legal only for the CPU backend)
        m_Processor->SetProcessingCompletedHandler([processor = m_Processor.get(), input, settings, callData = *processImageFile, payload = std::move(payload)](imppg::backend::CompletionStatus) {
            c_Image resultR = processor->GetProcessedOutput();

            // need to copy these for later use - the lambda we're currently executing has been assigned as the completion hander of processor,
            // and calling `SetProcessingCompletedHandler` below with a different lambda will immediately destroy our state variables
            const auto input2 = input;
            const auto settings2 = settings;
            const auto processor2 = processor;

            processor->SetProcessingCompletedHandler([processor, input, settings, callData, payload = std::move(payload), resultR = std::move(resultR)](imppg::backend::CompletionStatus) {
                c_Image resultG = processor->GetProcessedOutput();

                const auto input2 = input;
                const auto settings2 = settings;
                const auto processor2 = processor;

                processor->SetProcessingCompletedHandler([processor, input, settings, callData, payload = std::move(payload), resultR = std::move(resultR), resultG = std::move(resultG)](imppg::backend::CompletionStatus) mutable {
                    c_Image resultB = processor->GetProcessedOutput();

                    c_Image resultRGB = c_Image::CombineRGB(resultR, resultG, resultB);
                    IMPPG_ASSERT(resultRGB.SaveToFile(callData.outputImagePath, OutputFormat::BMP_8));

                    payload.SignalCompletion(call_result::Success{});
                });

                processor2->StartProcessing(std::make_shared<const c_Image>(std::get<2>(input2)), settings2);
            });

            processor2->StartProcessing(std::make_shared<const c_Image>(std::get<1>(input2)), settings2);
        });

        m_Processor->StartProcessing(std::make_shared<const c_Image>(std::get<0>(input)), settings);
    }
}

}
