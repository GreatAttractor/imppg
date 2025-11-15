/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Batch progress dialog implementation.
*/

#include <limits.h>
#include <optional>
#include <string>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/gauge.h>
#include <wx/grid.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>

#include "appconfig.h"
#include "backend/backend.h"
#include "batch_params.h"
#include "batch.h"
#include "ctrl_ids.h"
#include "image/image.h"
#include "common/imppg_assert.h"
#include "logging.h"
#include "common/proc_settings.h"

using namespace imppg::backend;

class c_BatchDialog: public wxDialog
{
    void OnCommandEvent(wxCommandEvent& event);
    void OnInit(wxInitDialogEvent& event);
    void OnIdle(wxIdleEvent& event);

    void InitControls();

    wxGrid m_Grid;
    wxArrayString m_FileNames;

    wxGauge* m_ProgressCtrl{nullptr};

    bool m_FileOperationFailure;

    struct
    {
        bool loadedSuccessfully;

        ProcessingSettings procSettings;

        wxString outputDir;
        OutputFormat outputFmt;
    } m_Settings;

    std::optional<size_t> m_CurrentFileIdx; ///< Index of the currently processed file.

    std::unique_ptr<imppg::backend::IProcessingBackEnd> m_Processor;

    bool m_ProcessNextFile{false};

    /// Starts processing of the next file
    void ProcessNextFile();

    /// Updates the progress string in the files grid
    void SetProgressInfo(wxString info);

    /// Returns 'false' on error
    bool SaveOutputFile();

    void OnProcessingCompleted(CompletionStatus status);

public:
    c_BatchDialog(
        wxWindow* parent,
        wxArrayString fileNames,
        wxString settingsFileName,
        wxString outputDirectory,
        OutputFormat outputFormat
    );

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(c_BatchDialog, wxDialog)
    EVT_BUTTON(ID_Close, c_BatchDialog::OnCommandEvent)
    EVT_INIT_DIALOG(c_BatchDialog::OnInit)
    EVT_IDLE(c_BatchDialog::OnIdle)
END_EVENT_TABLE()

/// Distance (in pixels) between controls
const int BORDER = 5;

void c_BatchDialog::OnInit(wxInitDialogEvent&)
{
    if (m_Settings.loadedSuccessfully)
    {
        ProcessNextFile();
    }
}

void c_BatchDialog::OnIdle(wxIdleEvent& event)
{
    if (!m_Settings.loadedSuccessfully || m_FileOperationFailure)
    {
        Close();
    }

    m_Processor->OnIdle(event);

    if (m_ProcessNextFile && m_CurrentFileIdx.has_value() && *m_CurrentFileIdx < m_FileNames.Count())
    {
        ProcessNextFile();
    }
}

/// Updates the progress string in the files grid
void c_BatchDialog::SetProgressInfo(wxString info)
{
    if (m_CurrentFileIdx.has_value())
    {
        m_Grid.SetCellValue(m_CurrentFileIdx.value(), 1, info);

        int newProgressColWidth = m_Grid.GetTextExtent(info).GetWidth() + 10;
        if (m_Grid.GetColSize(1) < newProgressColWidth)
            m_Grid.SetColSize(1, newProgressColWidth);
    }
}

/// Returns 'false' on error
bool c_BatchDialog::SaveOutputFile()
{
    wxFileName fn(m_FileNames[m_CurrentFileIdx.value()]);
    switch (m_Settings.outputFmt)
    {
    case OutputFormat::BMP_8: fn.SetExt("bmp"); break;
#if USE_FREEIMAGE
    case OutputFormat::PNG_8: fn.SetExt("png"); break;
#endif

    case OutputFormat::TIFF_16:
#if (USE_FREEIMAGE)
    case OutputFormat::TIFF_8_LZW:
    case OutputFormat::TIFF_16_ZIP:
    case OutputFormat::TIFF_32F:
    case OutputFormat::TIFF_32F_ZIP:
#endif
        fn.SetExt("tif");
        break;

#if USE_CFITSIO
    case OutputFormat::FITS_8:
    case OutputFormat::FITS_16:
    case OutputFormat::FITS_32F:
        fn.SetExt("fit");
        break;
#endif

    default: break;
    }

    wxString destPath = wxFileName(m_Settings.outputDir, fn.GetName() + "_out", fn.GetExt()).GetFullPath();
    if (!m_Processor->GetProcessedOutput().SaveToFile(ToFsPath(destPath), m_Settings.outputFmt))
    {
        wxMessageBox(wxString::Format(_("Could not save output file: %s"), destPath), _("Error"), wxICON_ERROR, this);
        m_FileOperationFailure = true;
        return false;
    }

    return true;
}

void c_BatchDialog::OnProcessingCompleted(CompletionStatus status)
{
    if (status == CompletionStatus::COMPLETED)
    {
        if (SaveOutputFile())
        {
            SetProgressInfo(_("Done"));
        }
        else
        {
            SetProgressInfo(_("Error"));
        }


        if (m_CurrentFileIdx.value() == m_FileNames.Count() - 1) // was it the last file?
        {
            m_ProgressCtrl->SetValue(m_FileNames.Count());
            m_CurrentFileIdx = std::nullopt;
            wxMessageBox(_("Processing completed."), _("Information"), wxICON_INFORMATION, this);
        }
        else
        {
            m_CurrentFileIdx.value() += 1;
            m_ProcessNextFile = true;
        }
    }
    else
    {
        SetProgressInfo("");
        m_CurrentFileIdx = std::nullopt;
    }
}

void c_BatchDialog::ProcessNextFile()
{
    m_ProcessNextFile = false;

    if (!m_CurrentFileIdx.has_value())
    {
        m_CurrentFileIdx = 0;
    }

    m_ProgressCtrl->SetValue(m_CurrentFileIdx.value());

    wxFileName path = wxFileName(m_FileNames[m_CurrentFileIdx.value()]);
    wxString ext = path.GetExt().Lower();
    std::string errorMsg;

    auto img = LoadImageFileAs32f(
        ToFsPath(path.GetFullPath()),
        Configuration::NormalizeFITSValues,
        &errorMsg
    );
    if (!img.has_value())
    {
        wxMessageBox(wxString::Format(_("Could not open file: %s."), path.GetFullPath()) + (errorMsg != "" ? "\n" + errorMsg : ""),
            _("Error"), wxICON_ERROR, this);
        m_FileOperationFailure = true;
        return;
    }

    const auto& proc = m_Settings.procSettings;
    if (proc.normalization.enabled)
    {
        NormalizeFpImage(img.value(), proc.normalization.min, proc.normalization.max);
    }

    m_Processor->StartProcessing(std::move(img.value()), proc);
}

void c_BatchDialog::OnCommandEvent(wxCommandEvent& event)
{
    switch (event.GetId())
    {
    case ID_Close:
        Close();
        break;
    }
}

c_BatchDialog::c_BatchDialog(wxWindow* parent, wxArrayString fileNames,
    wxString settingsFileName,
    wxString outputDirectory,
    OutputFormat outputFormat
)
: wxDialog(parent, wxID_ANY, _("Batch processing"), wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    switch (Configuration::ProcessingBackEnd)
    {
    case BackEnd::CPU_AND_BITMAPS: m_Processor = imppg::backend::CreateCpuBmpProcessingBackend(); break;
#if USE_OPENGL_BACKEND
    case BackEnd::GPU_OPENGL: m_Processor = imppg::backend::CreateOpenGLProcessingBackend(Configuration::LRCmdBatchSizeMpixIters); break;
#endif
    default: IMPPG_ABORT();
    }

    m_Processor->SetProgressTextHandler([this](wxString info) { SetProgressInfo(info); });
    m_Processor->SetProcessingCompletedHandler([this](CompletionStatus status) { OnProcessingCompleted(status); });

    m_FileOperationFailure = false;

    m_FileNames = std::move(fileNames);

    const auto settings = LoadSettings(settingsFileName);
    if (!settings.has_value())
    {
        wxMessageBox(_("Could not load processing settings."), _("Error"), wxICON_ERROR, parent);
        m_Settings.loadedSuccessfully = false;
    }
    else
    {
        m_Settings.procSettings = *settings;
        m_Settings.loadedSuccessfully = true;
    }

    m_Settings.outputDir = outputDirectory;
    m_Settings.outputFmt = outputFormat;

    InitControls();
}

void c_BatchDialog::InitControls()
{
    wxBoxSizer* szTop = new wxBoxSizer(wxVERTICAL);

    szTop->Add(m_ProgressCtrl = new wxGauge(this, ID_ProgressGauge, m_FileNames.Count(), wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL),
        0, wxALIGN_CENTER | wxALL | wxGROW, BORDER);

    m_Grid.Create(this, ID_Grid);
    m_Grid.CreateGrid(m_FileNames.Count(), 2, wxGrid::wxGridSelectRows);
    m_Grid.EnableEditing(false);
    m_Grid.DisableDragRowSize();
    m_Grid.HideRowLabels();
    m_Grid.SetColLabelValue(0, _("File"));
    m_Grid.SetColLabelValue(1, _("Progress"));
    for (size_t i = 0; i < m_FileNames.Count(); i++)
	{
        m_Grid.SetCellValue(i, 0, m_FileNames[i]);
		m_Grid.SetCellValue(i, 1, _("Waiting"));
	}
    m_Grid.AutoSizeColumns();
    szTop->Add(&m_Grid, 1, wxALIGN_CENTER | wxALL | wxGROW, BORDER);

    szTop->Add(new wxStaticLine(this), 0, wxGROW | wxALL, BORDER);

    szTop->Add(new wxButton(this, ID_Close, _("Close")), 0, wxALIGN_RIGHT | wxALL, BORDER);

    SetSizer(szTop);
    Fit();
}

/// Allows the user to choose images to process, choose the processing settings to use and displays the batch processing progress and control dialog
void BatchProcessing(
    wxWindow* parent ///< Pointer to the application's main window
)
{
    c_BatchParamsDialog batchParamsDlg(parent);
    if (batchParamsDlg.ShowModal() == wxID_OK)
    {
        c_BatchDialog batchDlg(
            parent,
            batchParamsDlg.GetInputFileNames(),
            batchParamsDlg.GetSettingsFileName(),
            batchParamsDlg.GetOutputDirectory(),
            batchParamsDlg.GetOutputFormat());
        wxRect r = Configuration::BatchProgressDialogPosSize;
        batchDlg.SetPosition(r.GetPosition());
        batchDlg.SetSize(r.GetSize());
        FixWindowPosition(batchDlg);
        batchDlg.ShowModal();
        Configuration::BatchProgressDialogPosSize = wxRect(batchDlg.GetPosition(), batchDlg.GetSize());
    }
    return;
}
