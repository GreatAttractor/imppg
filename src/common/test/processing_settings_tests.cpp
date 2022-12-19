#include "common/proc_settings.h"

#include <boost/test/unit_test.hpp>
#include <wx/sstream.h>

BOOST_AUTO_TEST_CASE(SaveAndLoadSettings)
{
    ProcessingSettings s{};
    s.LucyRichardson.deringing.enabled = true;
    s.LucyRichardson.iterations = 10;
    s.LucyRichardson.sigma = 1.5;

    s.normalization.enabled = true;
    s.normalization.min = 0.25;
    s.normalization.max = 0.75;

    auto& um = s.unsharpMask.at(0);
    um.adaptive = true;
    um.amountMax = 1.5;
    um.amountMin = 0.5;
    um.threshold = 0.25;
    um.width = 0.125;
    um.sigma = 1.5;

    s.toneCurve.UpdatePoint(0, 0.25, 0.25);
    s.toneCurve.UpdatePoint(1, 0.75, 0.5);

    wxStringOutputStream sOut;
    SerializeSettings(s, sOut);

    wxStringInputStream sIn(sOut.GetString());
    const auto loaded = DeserializeSettings(sIn);
    BOOST_REQUIRE(loaded.has_value());
    BOOST_REQUIRE(*loaded == s);
}

BOOST_AUTO_TEST_CASE(LoadLegacySettingsWithCommas)
{
    BOOST_CHECK_EQUAL(2, 3);
}
