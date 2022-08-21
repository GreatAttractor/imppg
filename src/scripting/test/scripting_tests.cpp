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

BOOST_FIXTURE_TEST_CASE(CallModuleAndSubmoduleFunctions, ScriptTestFixture)
{
    const char* script = R"(

imppg.test.notify_string("s1")
imppg.test.test.notify_string("s2")

    )";

    RunScript(script);
    CheckStringNotifications({"s1", "s2"});
}

BOOST_FIXTURE_TEST_CASE(NewSettingsConstructed_DefaultValues, ScriptTestFixture)
{
    const char* script = R"(

s = imppg.new_settings()
imppg.test.notify_settings(s)

    )";

    RunScript(script);

    const ProcessingSettings& settings = GetSettingsNotification();
    BOOST_CHECK_EQUAL(0, settings.LucyRichardson.iterations);
    BOOST_CHECK_EQUAL(false, settings.LucyRichardson.deringing.enabled);
    BOOST_CHECK_EQUAL(false, settings.unsharpMasking.adaptive);
    BOOST_CHECK_EQUAL(1.0, settings.unsharpMasking.amountMax);
    BOOST_CHECK_EQUAL(2, settings.toneCurve.GetNumPoints());
    BOOST_CHECK_EQUAL(0.0, settings.toneCurve.GetPoint(0).x);
    BOOST_CHECK_EQUAL(0.0, settings.toneCurve.GetPoint(0).y);
    BOOST_CHECK_EQUAL(1.0, settings.toneCurve.GetPoint(1).x);
    BOOST_CHECK_EQUAL(1.0, settings.toneCurve.GetPoint(1).y);
}
