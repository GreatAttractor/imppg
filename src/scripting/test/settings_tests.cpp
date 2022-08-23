#include "ScriptTestFixture.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_CASE(NewSettingsConstructed_CallGetters, ScriptTestFixture)
{
    const char* script = R"(

s = imppg.new_settings()
imppg.test.notify_number(s:get_unsh_mask_sigma())
-- TODO: add more getter calls

    )";

    RunScript(script);

    CheckNumberNotifications({1.0});
}
