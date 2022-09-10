#include "ScriptTestFixture.h"
#include "scripting/interop.h"
#include "scripting/script_runner.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread> //TESTING ##############
#include <wx/app.h>
#include <wx/init.h>

ScriptTestFixture::ScriptTestFixture()
{
    wxInitialize();
    m_App = std::make_unique<wxAppConsole>();
    m_App->Bind(wxEVT_THREAD, &ScriptTestFixture::OnRunnerMessage, this);
}

ScriptTestFixture::~ScriptTestFixture()
{
    m_App.reset();
    wxUninitialize();
    for (const auto& filePath: m_TemporaryFiles)
    {
        std::filesystem::remove(filePath);
    }
}

bool ScriptTestFixture::RunScript(const char* scriptText)
{
    std::promise<void> stopScript;
    scripting::ScriptRunner runner(std::make_unique<std::stringstream>(scriptText), *m_App, stopScript.get_future());
    runner.Run();
    m_App->MainLoop();
    runner.Wait();
    return !m_ScriptExecutionFailure;
}

void ScriptTestFixture::CreateFile(const std::filesystem::path& path)
{
    std::ofstream{path};
    m_TemporaryFiles.push_back(path);
}

void ScriptTestFixture::OnRunnerMessage(wxThreadEvent& event)
{
    switch (event.GetId())
    {
    case scripting::MessageId::ScriptFunctionCall:
    {
        auto payload = event.GetPayload<scripting::ScriptMessagePayload>();
        OnScriptFunctionCall(payload);
        break;
    }

    case scripting::MessageId::ScriptError:
    {
        auto payload = event.GetPayload<scripting::ScriptMessagePayload>();
        BOOST_TEST_MESSAGE("Script execution error: " << payload.GetMessage() << ".");
        m_ScriptExecutionFailure = true;
        break;
    }

    case scripting::MessageId::ScriptFinished:
        m_App->ExitMainLoop();
        break;

    default: break;
    }
}

void ScriptTestFixture::OnScriptFunctionCall(scripting::ScriptMessagePayload& payload)
{
    auto& callVariant = payload.GetCall();
    auto result = scripting::call_result::Success{};

    if (const auto* call = std::get_if<scripting::call::NotifyString>(&callVariant))
    {
        m_StringNotifications[call->s] += 1;
    }
    else if (auto* call = std::get_if<scripting::call::NotifySettings>(&callVariant))
    {
        m_SettingsNotification = call->settings;
    }
    else if (auto* call = std::get_if<scripting::call::NotifyImage>(&callVariant))
    {
        m_ImageNotification = call->image;
    }
    else if (auto* call = std::get_if<scripting::call::NotifyNumber>(&callVariant))
    {
        m_NumberNotifications.push_back(call->number);
    }
    else if (auto* call = std::get_if<scripting::call::NotifyBoolean>(&callVariant))
    {
        m_BooleanNotifications.push_back(call->value);
    }
    else if (auto* call = std::get_if<scripting::call::NotifyInteger>(&callVariant))
    {
        m_IntegerNotifications.push_back(call->value);
    }

    payload.SignalCompletion(std::move(result));
}

void ScriptTestFixture::CheckStringNotifications(std::initializer_list<std::string> expected) const
{
    for (const auto& expectedStr: expected)
    {
        BOOST_TEST_INFO("checking: " << expectedStr);
        const auto iter = m_StringNotifications.find(expectedStr);
        if (iter == m_StringNotifications.end())
        {
            BOOST_ERROR("not found");
        }
        else
        {
            BOOST_REQUIRE_EQUAL(static_cast<std::size_t>(1), iter->second);
        }
    }
}

const ProcessingSettings& ScriptTestFixture::GetSettingsNotification() const
{
    BOOST_REQUIRE(m_SettingsNotification.has_value());
    return *m_SettingsNotification;
}

const c_Image& ScriptTestFixture::GetImageNotification() const
{
    BOOST_REQUIRE(nullptr != m_ImageNotification);
    return *m_ImageNotification;
}

void ScriptTestFixture::CheckNumberNotifications(std::initializer_list<double> expected) const
{
    BOOST_REQUIRE_EQUAL(expected.size(), m_NumberNotifications.size());
    std::size_t index{0};
    for (const auto& expectedNumber: expected)
    {
        BOOST_CHECK_EQUAL(expectedNumber, m_NumberNotifications[index]);
        ++index;
    }
}

void ScriptTestFixture::CheckBooleanNotifications(std::initializer_list<bool> expected) const
{
    BOOST_REQUIRE_EQUAL(expected.size(), m_BooleanNotifications.size());
    std::size_t index{0};
    for (const auto& expectedValue: expected)
    {
        BOOST_CHECK_EQUAL(expectedValue, m_BooleanNotifications[index]);
        ++index;
    }
}

void ScriptTestFixture::CheckIntegerNotifications(std::initializer_list<int> expected) const
{
    BOOST_REQUIRE_EQUAL(expected.size(), m_IntegerNotifications.size());
    std::size_t index{0};
    for (const auto& expectedValue: expected)
    {
        BOOST_CHECK_EQUAL(expectedValue, m_IntegerNotifications[index]);
        ++index;
    }
}
