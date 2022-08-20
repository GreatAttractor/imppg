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

void ScriptTestFixture::RunScript(const char* scriptText)
{
    std::promise<void> stopScript;
    scripting::c_ScriptRunner runner(std::make_unique<std::stringstream>(scriptText), *m_App, stopScript.get_future());
    runner.Run();
    m_App->MainLoop();
    runner.Wait();
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

    case scripting::MessageId::ScriptFinished:
        m_App->ExitMainLoop();
        break;

    default: break;
    }
}

void ScriptTestFixture::OnScriptFunctionCall(scripting::ScriptMessagePayload& payload)
{
    //TODO: use visitor
    if (const auto* call = std::get_if<scripting::call::NotifyString>(&payload.GetCall()))
    {
        m_StringNotifications[call->s] += 1;
    }
    else if (const auto* call = std::get_if<scripting::call::NotifySettings>(&payload.GetCall()))
    {
        m_SettingsNotification = call->settings;
    }

    payload.SignalCompletion();
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
    return m_SettingsNotification;
}
