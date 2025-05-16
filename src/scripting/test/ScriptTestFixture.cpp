#include "backend/backend.h"
#include "ScriptTestFixture.h"
#include "scripting/interop.h"
#include "scripting/script_runner.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <wx/app.h>
#include <wx/init.h>

std::filesystem::path GetTestRoot()
{
    const auto root = std::filesystem::temp_directory_path() / "imppg_tests";
    std::filesystem::create_directories(root);
    return root;
}

ScriptTestFixture::ScriptTestFixture()
{
    wxInitialize();

    m_Processor =
        std::make_unique<scripting::ScriptImageProcessor>(imppg::backend::CreateCpuBmpProcessingBackend(), false);

    m_App = std::make_unique<wxAppConsole>();
    m_App->Bind(wxEVT_THREAD, &ScriptTestFixture::OnRunnerMessage, this);
    m_App->Bind(wxEVT_IDLE, &scripting::ScriptImageProcessor::OnIdle, m_Processor.get());
}

ScriptTestFixture::~ScriptTestFixture()
{
    m_Processor.reset(); // contained `wxThread`s must be destroyed before `wxUninitialize`
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

void ScriptTestFixture::CreateEmptyFile(const std::filesystem::path& path)
{
    std::ofstream{path.string().c_str()};
    m_TemporaryFiles.push_back(path);
}

void ScriptTestFixture::OnRunnerMessage(wxThreadEvent& event)
{
    auto payload = event.GetPayload<scripting::ScriptMessagePayload>();

    const auto handler = Overload{
        [&](const scripting::contents::None&) {},

        [&](const scripting::contents::ScriptFinished&) {
            m_App->ExitMainLoop();
        },

        [&](const scripting::contents::Error& contents) {
            auto payload = event.GetPayload<scripting::ScriptMessagePayload>();
            BOOST_TEST_MESSAGE("Script execution error: " << contents.message << ".");
            m_ScriptExecutionFailure = true;
        },

        [&](const auto&) {
            OnScriptMessageContents(payload);
        }
    };

    std::visit(handler, payload.GetContents());
}

void ScriptTestFixture::OnScriptMessageContents(scripting::ScriptMessagePayload& payload)
{
    const auto returnOk = [&]() { payload.SignalCompletion(scripting::call_result::Success{}); };

    const auto handler = Overload{
        [&](const scripting::contents::None&) {},

        [&](const scripting::contents::NotifyString& contents) {
            if (contents.ordered)
            {
                m_StringNotifications.push_back(contents.s);
            }
            else
            {
                m_StringNotificationsUnordered[contents.s] += 1;
            }
            returnOk();
        },

        [&](const scripting::contents::NotifySettings& contents) {
            m_SettingsNotification = contents.settings;
            returnOk();
        },

        [&](const scripting::contents::NotifyImage& contents) {
            m_ImageNotification = contents.image;
            returnOk();
        },

        [&](const scripting::contents::NotifyNumber& contents) {
            m_NumberNotifications.push_back(contents.number);
            returnOk();
        },

        [&](const scripting::contents::NotifyBoolean& contents) {
            m_BooleanNotifications.push_back(contents.value);
            returnOk();
        },

        [&](const scripting::contents::NotifyInteger& contents) {
            m_IntegerNotifications.push_back(contents.value);
            returnOk();
        },

        [&, this](const auto&) {
            // We need to make copies first, because in `StartProcessing` invocation we also move from `payload`,
            // and function argument evaluation order is unspecified.
            scripting::MessageContents contentsCopy = payload.GetContents();
            auto heartbeat = payload.GetHeartbeat();
            m_Processor->StartProcessing(
                std::move(contentsCopy),
                heartbeat,
                [payload = std::move(payload)](scripting::FunctionCallResult result) mutable {
                    payload.SignalCompletion(std::move(result));
                }
            );
        }
    };

    std::visit(handler, payload.GetContents());
}

void ScriptTestFixture::CheckStringNotifications(std::initializer_list<std::string> expected) const
{
    BOOST_REQUIRE(expected.size() == m_StringNotifications.size());
    std::size_t idx{0};
    for (const auto& expectedStr: expected)
    {
        BOOST_CHECK_EQUAL(expectedStr, m_StringNotifications[idx]);
        ++idx;
    }
}

void ScriptTestFixture::CheckUnorderedStringNotifications(std::initializer_list<std::string> expected) const
{
    for (const auto& expectedStr: expected)
    {
        BOOST_TEST_INFO("checking: " << expectedStr);
        const auto iter = m_StringNotificationsUnordered.find(expectedStr);
        if (iter == m_StringNotificationsUnordered.end())
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
