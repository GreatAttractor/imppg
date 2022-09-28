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
    Script interoperability header.
*/

#pragma once

#include "common/formats.h"
#include "common/proc_settings.h"
#include "image/image.h"
#include "scripting/script_exceptions.h"

#include <future>
#include <lua.hpp>
#include <memory>
#include <variant>
#include <wx/event.h>

namespace scripting
{

namespace contents
{
struct None {};
struct ScriptFinished {};
struct Error{ std::string message; };
struct NotifyBoolean { bool value; };
struct NotifyImage { std::shared_ptr<const c_Image> image; };
struct NotifyInteger { int value; };
struct NotifyNumber { double number; };
struct NotifySettings { ProcessingSettings settings; };
struct NotifyString { std::string s; };
struct Progress { int percentage; };

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

}

using MessageContents = std::variant<
    contents::AlignRGB,
    contents::Error,
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

/// Payload of messages sent by script runner's worker thread to parent.
///
/// The default constructor and fake copying - which in fact moves - are required due to `wxThreadEvent`'s needs.
///
class ScriptMessagePayload
{
public:
    ScriptMessagePayload()
    : m_Contents(contents::None{})
    {}

    ScriptMessagePayload(MessageContents&& contents, std::promise<FunctionCallResult>&& completion = {})
    : m_Contents(std::move(contents)), m_Completion(std::move(completion))
    {}

    ScriptMessagePayload(const ScriptMessagePayload& other)
    {
        if (&other != this)
        {
            *this = std::move(other);
        }
    }

    ScriptMessagePayload& operator=(const ScriptMessagePayload& other)
    {
        if (&other != this)
        {
            *this = std::move(const_cast<ScriptMessagePayload&>(other));
        }
        return *this;
    }

    ScriptMessagePayload(ScriptMessagePayload&& other) = default;
    ScriptMessagePayload& operator=(ScriptMessagePayload&& other) = default;

    const MessageContents& GetContents() const { return m_Contents; }

    void SignalCompletion(FunctionCallResult&& result) { m_Completion.set_value(std::move(result)); }

private:
    MessageContents m_Contents;

    /// Empty for certain kinds of `MessageContents`; the receiver must then not call `SignalCompletion`.
    std::promise<FunctionCallResult> m_Completion;
};

/// Prepares interop for script execution.
void Prepare(lua_State* lua, wxEvtHandler& parent, std::future<void>&& stopRequested);

/// Cleans up interop state.
void Finish();

}
