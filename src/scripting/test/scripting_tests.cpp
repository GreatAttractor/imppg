#include "ScriptTestFixture.h"

#include <boost/test/unit_test.hpp>
#include <future>
#include <memory>

BOOST_AUTO_TEST_SUITE(Dummy);

BOOST_AUTO_TEST_CASE(Dummy1)
{
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_SUITE_END();

BOOST_FIXTURE_TEST_CASE(ModulesAndSubmodulesRegistered_CallModuleFunction_CallSucceeds, ScriptTestFixture)
{
    const char* script = R"(

imppg.test.notify_string("s1")
imppg.test.test.notify_string("s2")

    )";

    RunScript(script);
    CheckStringNotifications({"s1", "s2"});
}
