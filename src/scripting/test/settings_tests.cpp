#include "ScriptTestFixture.h"

#include <boost/test/unit_test.hpp>

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
    BOOST_CHECK_EQUAL(false, settings.unsharpMask.at(0).adaptive);
    BOOST_CHECK_EQUAL(1.0, settings.unsharpMask.at(0).amountMax);
    BOOST_CHECK_EQUAL(2, settings.toneCurve.GetNumPoints());
    BOOST_CHECK_EQUAL(0.0, settings.toneCurve.GetPoint(0).x);
    BOOST_CHECK_EQUAL(0.0, settings.toneCurve.GetPoint(0).y);
    BOOST_CHECK_EQUAL(1.0, settings.toneCurve.GetPoint(1).x);
    BOOST_CHECK_EQUAL(1.0, settings.toneCurve.GetPoint(1).y);
}

BOOST_FIXTURE_TEST_CASE(CallSettingsValuesSetters_GettersReturnSameValues, ScriptTestFixture)
{
    const char* script = R"(

s = imppg.new_settings()

s:normalization_enabled(true)
s:normalization_min(0.25)
s:normalization_max(0.75)

s:lr_deconv_sigma(5.0)
s:lr_deconv_num_iters(123)
s:lr_deconv_deringing(true)

s:unsh_mask_adaptive(0, true)
s:unsh_mask_sigma(0, 5.0)
s:unsh_mask_amount_min(0, 1.0)
s:unsh_mask_amount_max(0, 2.0)
s:unsh_mask_threshold(0, 0.125)
s:unsh_mask_twidth(0, 0.125)

-- TODO: tone curve setters & getters

imppg.test.notify_boolean(s:get_normalization_enabled())
imppg.test.notify_number(s:get_normalization_min())
imppg.test.notify_number(s:get_normalization_max())

imppg.test.notify_number(s:get_lr_deconv_sigma())
imppg.test.notify_integer(s:get_lr_deconv_num_iters())
imppg.test.notify_boolean(s:get_lr_deconv_deringing())

imppg.test.notify_boolean(s:get_unsh_mask_adaptive(0))
imppg.test.notify_number(s:get_unsh_mask_sigma(0))
imppg.test.notify_number(s:get_unsh_mask_amount_min(0))
imppg.test.notify_number(s:get_unsh_mask_amount_max(0))
imppg.test.notify_number(s:get_unsh_mask_amount(0))
imppg.test.notify_number(s:get_unsh_mask_threshold(0))
imppg.test.notify_number(s:get_unsh_mask_twidth(0))

    )";

    RunScript(script);

    CheckBooleanNotifications({true, true, true});
    CheckNumberNotifications({0.25, 0.75, 5.0, 5.0, 1.0, 2.0, 2.0, 0.125, 0.125});
    CheckIntegerNotifications({123});
}

BOOST_FIXTURE_TEST_CASE(AddTwoToneCurvePointsWithSameX_ExecutionFails, ScriptTestFixture)
{
    const char* script = R"(

s = imppg.new_settings()
s:tc_add_point(0.5, 0)
s:tc_add_point(0.5, 0)

    )";

    BOOST_CHECK(!RunScript(script));
}

BOOST_FIXTURE_TEST_CASE(ModifyToneCurvePointToOverlapWithSuccessor_ExecutionFails, ScriptTestFixture)
{
    const char* script = R"(

s = imppg.new_settings()
new_idx1 = s:tc_add_point(0.1, 0)
new_idx2 = s:tc_add_point(0.2, 0)
s:tc_set_point(new_idx1, 0.2, 0)

    )";

    BOOST_CHECK(!RunScript(script));
}

BOOST_FIXTURE_TEST_CASE(ModifyToneCurvePointToOverlapWithPredecessor_ExecutionFails, ScriptTestFixture)
{
    const char* script = R"(

s = imppg.new_settings()
new_idx1 = s:tc_add_point(0.1, 0)
new_idx2 = s:tc_add_point(0.2, 0)
s:tc_set_point(new_idx2, 0.1, 0)

    )";

    BOOST_CHECK(!RunScript(script));
}
