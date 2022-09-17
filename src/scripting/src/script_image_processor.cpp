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
    Script image processor implementation.
*/

#include "../../imppg_assert.h"
#include "common/proc_settings.h"
#include "scripting/script_image_processor.h"

#include <wx/intl.h>

// private definitions
namespace
{

template<typename ... Ts>
struct Overload : Ts ... {
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

}

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

void ScriptImageProcessor::StartProcessing(
    MessageContents request,
    std::function<void(FunctionCallResult)> onCompletion
)
{
    std::optional<wxString> errorMsg = std::nullopt;

    const auto handler = Overload{
        [&, this](const contents::ProcessImageFile& call) {
            std::string loadErrorMsg;
            auto loadResult = LoadImageFileAsMono32f(call.imagePath, m_NormalizeFitsValues, &loadErrorMsg);
            if (!loadResult)
            {
                onCompletion(call_result::Error{
                    wxString::Format(_("failed to load image from %s; %s"), call.imagePath, loadErrorMsg).ToStdString()
                });
                return;
            }
            auto image = std::make_shared<const c_Image>(std::move(loadResult.value()));

            ProcessingSettings settings{};
            if (!LoadSettings(call.settingsPath, settings))
            {
                onCompletion(call_result::Error{
                    wxString::Format(_("failed to load processing settings from %s"), call.settingsPath).ToStdString()
                });
                return;
            }

            m_Processor->SetProcessingCompletedHandler(
                [this, onCompletion = std::move(onCompletion), outPath = call.outputImagePath, outFmt = call.outputFormat]
                    (imppg::backend::CompletionStatus) {

                        if (!m_Processor->GetProcessedOutput().SaveToFile(outPath, outFmt))
                        {
                            onCompletion(call_result::Error{
                                wxString::Format(_("failed to save output file %s"), outPath).ToStdString()
                            });
                        }
                        else
                        {
                            onCompletion(call_result::Success{});
                        }
                }
            );
            m_Processor->StartProcessing(image, settings);
        },

        [&](const contents::ProcessImage& call) {

            m_Processor->SetProcessingCompletedHandler(
                [onCompletion = std::move(onCompletion), this](imppg::backend::CompletionStatus) {
                    onCompletion(call_result::ImageProcessed{
                        std::make_shared<const c_Image>(m_Processor->GetProcessedOutput())
                    });
                }
            );
            m_Processor->StartProcessing(call.image, call.settings);
        },

        [](auto) { IMPPG_ABORT_MSG("invalid message passed to ScriptImageProcessor"); },
    };

    std::visit(handler, request);
}

void ScriptImageProcessor::OnIdle(wxIdleEvent& event)
{
    m_Processor->OnIdle(event);
}

}
