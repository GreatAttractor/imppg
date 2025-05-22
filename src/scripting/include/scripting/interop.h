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
    Script interoperability header.
*/

#pragma once

#include "alignment/align_proc.h"
#include "common/formats.h"
#include "common/proc_settings.h"
#include "image/image.h"
#include "scripting/script_exceptions.h"

#include <wx/msgqueue.h>

#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <variant>

namespace scripting
{

namespace contents
{
struct None {};
struct ScriptFinished {};
struct Error { std::string message; };
struct Warning { std::string message; };
struct NotifyBoolean { bool value; };
struct NotifyImage { std::shared_ptr<const c_Image> image; };
struct NotifyInteger { int value; };
struct NotifyNumber { double number; };
struct NotifySettings { ProcessingSettings settings; };
struct NotifyString { std::string s; bool ordered; };
struct Progress { double fraction; };
struct PrintMessage { std::string message; };

struct AlignRGB
{
    // the fields are always filled; `optional` is required for default-constructability (wxThreadEvent requires it)
    std::optional<c_Image> red, green, blue;
};

struct ProcessImageFile
{
    std::string imagePath;
    std::string settingsPath;
    std::string outputImagePath;
    OutputFormat outputFormat;
};

struct ProcessImage
{
    std::shared_ptr<const c_Image> image;
    ProcessingSettings settings;
};

struct AlignImages
{
    std::vector<std::filesystem::path> inputFiles;
    AlignmentMethod alignMode;
    CropMode cropMode;
    bool subpixelAlignment;
    std::filesystem::path outputDir;
    std::optional<std::string> outputFNameSuffix;
    std::function<void(double)> progressCallback;
};

}

using MessageContents = std::variant<
    contents::AlignImages,
    contents::AlignRGB,
    contents::Error,
    contents::Warning,
    contents::None,
    contents::NotifyBoolean,
    contents::NotifyImage,
    contents::NotifyInteger,
    contents::NotifyNumber,
    contents::NotifySettings,
    contents::NotifyString,
    contents::ProcessImage,
    contents::ProcessImageFile,
    contents::Progress,
    contents::PrintMessage,
    contents::ScriptFinished
>;

namespace call_result
{
struct Success {};
struct ImageProcessed { std::shared_ptr<c_Image> image; };
struct Error { std::string message; };
}

using FunctionCallResult = std::variant<
    call_result::Success,
    call_result::ImageProcessed,
    call_result::Error
>;

/// Notifies about function still running (by sending `std::nullopt`) or its completion.
using Heartbeat = wxMessageQueue<std::optional<FunctionCallResult>>;

/// Payload of messages sent by script runner's worker thread to parent.
///
/// Using `std::shared_ptr` for `m_Completion` is required due to `wxThreadEvent`'s needs.
///
class ScriptMessagePayload
{
public:
    ScriptMessagePayload()
    : m_Contents(contents::None{})
    {}

    ScriptMessagePayload(MessageContents&& contents, std::shared_ptr<Heartbeat> heartbeat = {})
    : m_Contents(std::move(contents)), m_Heartbeat(std::move(heartbeat))
    {}

    const MessageContents& GetContents() const { return m_Contents; }

    std::shared_ptr<Heartbeat> GetHeartbeat() const { return m_Heartbeat; }

    void SignalOperationInProgress()
    {
        if (!m_Heartbeat) { throw std::logic_error{"no heartbeat associated with this message payload"}; }
        m_Heartbeat->Post(std::nullopt);
    }

    void SignalCompletion(FunctionCallResult&& result)
    {
        if (!m_Heartbeat) { throw std::logic_error{"no heartbeat associated with this message payload"}; }
        m_Heartbeat->Post(std::move(result));
    }

private:
    MessageContents m_Contents;

    /// Unused for certain kinds of `MessageContents`.
    std::shared_ptr<Heartbeat> m_Heartbeat;
};

}
