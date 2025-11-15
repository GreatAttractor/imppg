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
    Script image processor implementation.
*/

#include "common/imppg_assert.h"
#include "alignment/align_proc.h"
#include "common/proc_settings.h"
#include "scripting/script_image_processor.h"

#include <filesystem>
#include <memory>
#include <tuple>
#include <wx/intl.h>

namespace scripting
{

ScriptImageProcessor::ScriptImageProcessor(
    std::unique_ptr<imppg::backend::IProcessingBackEnd>&& processor,
    bool normalizeFitsValues
)
: m_Processor(std::move(processor))
, m_NormalizeFitsValues(normalizeFitsValues)
{
}

ScriptImageProcessor::~ScriptImageProcessor()
{}

void ScriptImageProcessor::StartProcessing(
    MessageContents request,
    std::shared_ptr<Heartbeat> heartbeat,
    std::function<void(FunctionCallResult)> onCompletion
)
{
    const auto handler = Overload{
        [&](const contents::ProcessImageFile& call) { OnProcessImageFile(call, onCompletion); },

        [&](const contents::ProcessImage& call) { OnProcessImage(call, onCompletion); },

        [&](const contents::AlignRGB& call) { OnAlignRGB(call, onCompletion); },

        [&](const contents::AlignImages& call) { OnAlignImages(call, heartbeat, onCompletion); },

        [](auto) { IMPPG_ABORT_MSG("invalid message passed to ScriptImageProcessor"); },
    };

    try
    {
        std::visit(handler, request);
    }
    catch(const std::exception& e)
    {
        onCompletion(call_result::Error{e.what()});
    }
}

void ScriptImageProcessor::OnIdle(wxIdleEvent& event)
{
    m_Processor->OnIdle(event);
}

void ScriptImageProcessor::OnProcessImageFile(const contents::ProcessImageFile& call, CompletionFunc onCompletion)
{
    std::string loadErrorMsg;
    auto loadResult = LoadImageFileAs32f(call.imagePath, m_NormalizeFitsValues, &loadErrorMsg);
    if (!loadResult)
    {
        onCompletion(call_result::Error{
            wxString::Format(_("failed to load image from %s; %s"), call.imagePath.native(), loadErrorMsg)
        });
        return;
    }
    c_Image image = std::move(loadResult.value());

    const auto settings = LoadSettings(call.settingsPath.native());
    if (!settings.has_value())
    {
        onCompletion(call_result::Error{
            wxString::Format(_("failed to load processing settings from %s"), call.settingsPath.native())
        });
        return;
    }

    if (settings->normalization.enabled)
    {
        NormalizeFpImage(image, settings->normalization.min, settings->normalization.max);
    }

    m_Processor->SetProcessingCompletedHandler(
        [this, onCompletion = std::move(onCompletion), outPath = call.outputImagePath, outFmt = call.outputFormat]
            (imppg::backend::CompletionStatus) {

                if (!m_Processor->GetProcessedOutput().SaveToFile(outPath, outFmt))
                {
                    onCompletion(call_result::Error{
                        wxString::Format(_("failed to save output file %s"), outPath.native())
                    });
                }
                else
                {
                    onCompletion(call_result::Success{});
                }
        }
    );
    m_Processor->StartProcessing(std::move(image), *settings);
}

void ScriptImageProcessor::OnProcessImage(const contents::ProcessImage& call, CompletionFunc onCompletion)
{
    c_Image image = *call.image;
    if (call.settings.normalization.enabled)
    {
        NormalizeFpImage(image, call.settings.normalization.min, call.settings.normalization.max);
    }

    m_Processor->SetProcessingCompletedHandler(
        [onCompletion = std::move(onCompletion), this](imppg::backend::CompletionStatus) {
            onCompletion(call_result::ImageProcessed{
                std::make_shared<c_Image>(m_Processor->GetProcessedOutput())
            });
        }
    );
    m_Processor->StartProcessing(std::move(image), call.settings);
}

static double CalculateProgress(
    std::size_t stage,
    std::size_t numStages,
    std::size_t imgIdx,
    std::size_t numImages
)
{
    IMPPG_ASSERT(numStages != 0 && numImages != 0);

    return static_cast<double>(stage * numImages + imgIdx) / (numStages * numImages);
}

void ScriptImageProcessor::OnAlignImages(
    const contents::AlignImages& call,
    std::shared_ptr<Heartbeat> heartbeat,
    CompletionFunc onCompletion
)
{
    IMPPG_ASSERT(heartbeat != nullptr);

    if (m_AlignmentWorker)
    {
        m_AlignmentWorker->Wait();
    }

    if (!std::filesystem::exists(call.outputDir))
    {
        onCompletion(call_result::Error{
            wxString::Format(_("output directory %s does not exist"), call.outputDir.generic_string())
        });
        return;
    }

    m_AlignmentEvtHandler = std::make_unique<wxEvtHandler>();
    m_AlignmentEvtHandler->Bind(wxEVT_THREAD,
        [
            onCompletion = std::move(onCompletion),
            alignMode = call.alignMode,
            progressCallback = std::move(call.progressCallback),
            numImages = call.inputFiles.size(),
            heartbeat
        ](wxThreadEvent& event) {
            const auto sendHeartbeat = [heartbeat]() { heartbeat->Post(std::nullopt); };

            const int imgIdx = event.GetInt();

            switch (event.GetId())
            {
            case EID_ABORTED:
                onCompletion(call_result::Error{event.GetString()});
                break;

            case EID_PHASECORR_IMG_TRANSLATION:
                progressCallback(CalculateProgress(0, 2, imgIdx, numImages));
                sendHeartbeat();
                break;

            case EID_SAVED_OUTPUT_IMAGE: {
                const auto [stageIdx, numStages] = [&]() {
                    switch (alignMode)
                    {
                    case AlignmentMethod::PHASE_CORRELATION: return std::make_tuple<std::size_t, std::size_t>(1, 2);
                    case AlignmentMethod::LIMB: return std::make_tuple<std::size_t, std::size_t>(2, 3);
                    default: IMPPG_ABORT_MSG("unrecognized alignment method");
                    }
                }();

                progressCallback(CalculateProgress(stageIdx, numStages, imgIdx, numImages));
                sendHeartbeat();
                } break;

            case EID_LIMB_FOUND_DISC_RADIUS:
                progressCallback(CalculateProgress(0, 3, imgIdx, numImages));
                sendHeartbeat();
                break;

            case EID_LIMB_STABILIZATION_PROGRESS:
                progressCallback(CalculateProgress(1, 3, imgIdx, numImages));
                sendHeartbeat();
                break;

            case EID_COMPLETED:
                if (event.GetString().empty())
                {
                    onCompletion(call_result::Success{});
                }
                else
                {
                    onCompletion(call_result::Error{event.GetString()});
                }
                break;


            default: break;
            }
        }
    );

    AlignmentParameters_t alignParams{};
    alignParams.alignmentMethod = call.alignMode;
    alignParams.subpixelAlignment = call.subpixelAlignment;
    alignParams.cropMode = call.cropMode;
    alignParams.inputs = [&]() {
        wxArrayString files;
        for (const auto& path: call.inputFiles)
        {
            files.Add(path.native());
        }
        return files;
    }();
    alignParams.normalizeFitsValues = false;
    alignParams.outputDir = call.outputDir.native();
    alignParams.outputFNameSuffix = call.outputFNameSuffix;

    m_AlignmentWorker = std::make_unique<c_ImageAlignmentWorkerThread>(*m_AlignmentEvtHandler, std::move(alignParams));
    m_AlignmentWorker->Run();
}

void ScriptImageProcessor::OnAlignRGB(const contents::AlignRGB& call, CompletionFunc onCompletion)
{
    if (m_AlignmentWorker)
    {
        m_AlignmentWorker->Wait();
    }

    m_AlignmentEvtHandler = std::make_unique<wxEvtHandler>(); //TODO: do it in a more elegant way
    m_AlignmentEvtHandler->Bind(wxEVT_THREAD, [
        onCompletion = std::move(onCompletion),
        red = call.red,
        green = call.green,
        blue = call.blue
    ](wxThreadEvent& event) {
        const auto eid = event.GetId();
        if (eid == EID_TRANSLATIONS)
        {
            auto translations = event.GetPayload<std::shared_ptr<std::vector<FloatPoint_t>>>();
            IMPPG_ASSERT(translations && translations->size() == 3);

            const auto& tR = (*translations)[0];
            const auto& tG = (*translations)[1];
            const auto& tB = (*translations)[2];
            const auto mid = FloatPoint_t{ (tR.x + tG.x + tB.x) / 3.0f, (tR.y + tB.y + tB.y) / 3.0f };

            const auto width = red->GetWidth();
            const auto height = red->GetHeight();
            const auto pixFmt = red->GetPixelFormat();
            c_Image alignedR(width, height, pixFmt);
            c_Image alignedG(width, height, pixFmt);
            c_Image alignedB(width, height, pixFmt);

            c_Image::ResizeAndTranslate(
                red->GetBuffer(),
                alignedR.GetBuffer(),
                0, 0,
                width - 1, height - 1,
                mid.x - tR.x, mid.y - tR.y,
                true
            );

            c_Image::ResizeAndTranslate(
                green->GetBuffer(),
                alignedG.GetBuffer(),
                0, 0,
                width - 1, height - 1,
                mid.x - tG.x, mid.y - tG.y,
                true
            );

            c_Image::ResizeAndTranslate(
                blue->GetBuffer(),
                alignedB.GetBuffer(),
                0, 0,
                width - 1, height - 1,
                mid.x - tB.x, mid.y - tB.y,
                true
            );

            c_Image combined = c_Image::CombineRGB(alignedR, alignedG, alignedB);
            onCompletion(call_result::ImageProcessed{std::make_shared<c_Image>(std::move(combined))});
        }
        else if (eid == EID_ABORTED)
        {
            onCompletion(call_result::Error{_("RGB alignment aborted")});
        }
    });

    AlignmentParameters_t alignParams{};
    alignParams.alignmentMethod = AlignmentMethod::PHASE_CORRELATION;
    alignParams.subpixelAlignment = true;
    alignParams.cropMode = CropMode::PAD_TO_BOUNDING_BOX;
    alignParams.inputs = InputImageList{
        std::make_shared<const c_Image>(std::move(call.red.value())),
        std::make_shared<const c_Image>(std::move(call.green.value())),
        std::make_shared<const c_Image>(std::move(call.blue.value()))
    };
    alignParams.normalizeFitsValues = false;

    m_AlignmentWorker = std::make_unique<c_ImageAlignmentWorkerThread>(*m_AlignmentEvtHandler, std::move(alignParams));
    m_AlignmentWorker->Run();
}

void ScriptImageProcessor::AbortProcessing()
{
    m_Processor->AbortProcessing();
    if (m_AlignmentWorker && m_AlignmentWorker->IsRunning())
    {
        m_AlignmentWorker->AbortProcessing();
        m_AlignmentWorker->Wait();
    }
}

}
