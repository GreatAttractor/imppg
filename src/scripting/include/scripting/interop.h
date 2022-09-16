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

namespace MessageId
{
    enum { ScriptFunctionCall, ScriptError, ScriptFinished };
}

namespace call
{
struct None {};
struct Dummy {};
struct NotifyBoolean { bool value; };
struct NotifyImage { std::shared_ptr<const c_Image> image; };
struct NotifyInteger { int value; };
struct NotifyNumber { double number; };
struct NotifySettings { ProcessingSettings settings; };
struct NotifyString { std::string s; };

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

using FunctionCall = std::variant<
    call::None,
    call::Dummy,
    call::NotifyBoolean,
    call::NotifyImage,
    call::NotifyInteger,
    call::NotifySettings,
    call::NotifyString,
    call::NotifyNumber,
    call::ProcessImage,
    call::ProcessImageFile
>;

namespace call_result
{
struct Success {};
struct ImageProcessed { std::shared_ptr<const c_Image> image; };
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
    : m_Call(call::None{})
    {}

    ScriptMessagePayload(FunctionCall call, std::promise<FunctionCallResult>&& completion)
    : m_Call(call), m_Completion(std::move(completion))
    {}

    ScriptMessagePayload(std::string message)
    : m_Message(std::move(message))
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

    const FunctionCall& GetCall() const { return m_Call; }

    void SignalCompletion(FunctionCallResult&& result) { m_Completion.set_value(std::move(result)); }

    const std::string& GetMessage() const { return m_Message; }

private:
    FunctionCall m_Call;
    std::promise<FunctionCallResult> m_Completion;
    std::string m_Message;
};

/// Prepares interop for script execution.
void Prepare(lua_State* lua, wxEvtHandler& parent);

/// Cleans up interop state.
void Finish();

}
