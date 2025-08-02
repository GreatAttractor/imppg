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

    UnsharpMask um2{};
    um2.adaptive = false;
    um2.amountMax = 1.25;
    um2.amountMin = 0.25;
    um2.threshold = 0.125;
    um2.width = 0.125;
    um2.sigma = 1.5;
    s.unsharpMask.push_back(um2);

    s.toneCurve.UpdatePoint(0, 0.25, 0.25);
    s.toneCurve.UpdatePoint(1, 0.75, 0.5);

    wxStringOutputStream sOut;
    SerializeSettings(s, sOut);

    wxStringInputStream sIn(sOut.GetString());
    const auto parsed = DeserializeSettings(sIn);
    BOOST_REQUIRE(parsed.has_value());
    BOOST_REQUIRE(*parsed == s);
}

BOOST_AUTO_TEST_CASE(LoadLegacySettingsWithOneUnsharpMaskAndCommas)
{
    const char* xml =
R"(<?xml version="1.0" encoding="UTF-8"?>
<imppg>
    <lucy-richardson sigma="1,5000" iterations="50" deringing="false"/>
    <unsharp_mask adaptive="false" sigma="1,25000" amount_min="1,0000" amount_max="1,5000" amount_threshold="0,25" amount_width="0,25"/>
    <tone_curve smooth="true" is_gamma="false">0,0000;0,0000;0,25;0,25;0,75;0,75;1,0000;1,0000;</tone_curve>
    <normalization enabled="true" min="0,25" max="0,75"/>
</imppg>
    )";

    const auto expected = ProcessingSettings{
        {true, 0.25, 0.75},
        {1.5, 50, {false}},
        {UnsharpMask{false, 1.25, 1.0, 1.5, 0.25, 0.25}},
        c_ToneCurve{{{0.0, 0.0}, {0.25, 0.25}, {0.75, 0.75}, {1.0, 1.0}}}
    };

    wxStringInputStream sIn(xml);
    const auto parsed = DeserializeSettings(sIn);
    BOOST_REQUIRE(parsed.has_value());
    BOOST_REQUIRE(*parsed == expected);
}

BOOST_AUTO_TEST_CASE(GivenInvalidToneCurveWithRepeatedInitialPoints_PointsAreMergedAndLoadSucceeds)
{
    const char* xml =
R"(<?xml version="1.0" encoding="UTF-8"?>
<imppg>
    <lucy-richardson sigma="1.5000" iterations="50" deringing="false"/>
    <unsharp_mask_list>
        <unsharp_mask adaptive="false" sigma="1.0" amount_min="1.0000" amount_max="1.0" amount_threshold="0.0100" amount_width="0.1000"/>
    </unsharp_mask_list>
    <tone_curve smooth="true" is_gamma="false">0.0000;0.0000;0.5;0.1;0.5;0.2;0.5;0.3;1,0000;1,0000;</tone_curve>
    <normalization enabled="false" min="0.0" max="0.0"/>
</imppg>
    )";

    wxStringInputStream sIn(xml);
    const auto parsed = DeserializeSettings(sIn);
    BOOST_REQUIRE(parsed.has_value());

    BOOST_REQUIRE_EQUAL(3, parsed->toneCurve.GetNumPoints());
    BOOST_REQUIRE_CLOSE(0.5,  parsed->toneCurve.GetPoint(1).x, 1.0e-5);
    BOOST_REQUIRE_CLOSE((0.1 + 0.2 + 0.3) / 3,  parsed->toneCurve.GetPoint(1).y, 1.0e-5);
}

BOOST_AUTO_TEST_CASE(GivenInvalidToneCurveWithRepeatedMiddlePoints_PointsAreMergedAndLoadSucceeds)
{
    const char* xml =
R"(<?xml version="1.0" encoding="UTF-8"?>
<imppg>
    <lucy-richardson sigma="1.5000" iterations="50" deringing="false"/>
    <unsharp_mask_list>
        <unsharp_mask adaptive="false" sigma="1.0" amount_min="1.0000" amount_max="1.0" amount_threshold="0.0100" amount_width="0.1000"/>
    </unsharp_mask_list>
    <tone_curve smooth="true" is_gamma="false">0.0000;0.0000;0.5;0.1;0.5;0.2;0.5;0.3;1,0000;1,0000;</tone_curve>
    <normalization enabled="false" min="0.0" max="0.0"/>
</imppg>
    )";

    wxStringInputStream sIn(xml);
    const auto parsed = DeserializeSettings(sIn);
    BOOST_REQUIRE(parsed.has_value());

    BOOST_REQUIRE_EQUAL(3, parsed->toneCurve.GetNumPoints());
    BOOST_REQUIRE_CLOSE(0.5,  parsed->toneCurve.GetPoint(1).x, 1.0e-5);
    BOOST_REQUIRE_CLOSE((0.1 + 0.2 + 0.3) / 3,  parsed->toneCurve.GetPoint(1).y, 1.0e-5);
}

BOOST_AUTO_TEST_CASE(GivenInvalidToneCurveWithRepeatedFinalPoints_PointsAreMergedAndLoadSucceeds)
{
    const char* xml =
R"(<?xml version="1.0" encoding="UTF-8"?>
<imppg>
    <lucy-richardson sigma="1.5000" iterations="50" deringing="false"/>
    <unsharp_mask_list>
        <unsharp_mask adaptive="false" sigma="1.0" amount_min="1.0000" amount_max="1.0" amount_threshold="0.0100" amount_width="0.1000"/>
    </unsharp_mask_list>
    <tone_curve smooth="true" is_gamma="false">0.0000;0.0000;0.5;0.1;0.5;0.2;0.5;0.3;1,0000;1,0000;</tone_curve>
    <normalization enabled="false" min="0.0" max="0.0"/>
</imppg>
    )";

    wxStringInputStream sIn(xml);
    const auto parsed = DeserializeSettings(sIn);
    BOOST_REQUIRE(parsed.has_value());

    BOOST_REQUIRE_EQUAL(3, parsed->toneCurve.GetNumPoints());
    BOOST_REQUIRE_CLOSE(0.5,  parsed->toneCurve.GetPoint(1).x, 1.0e-5);
    BOOST_REQUIRE_CLOSE((0.1 + 0.2 + 0.3) / 3,  parsed->toneCurve.GetPoint(1).y, 1.0e-5);
}

BOOST_AUTO_TEST_CASE(GivenInvalidToneCurveWithOnlyTwoRepeatedPoints_LoadFails)
{
    const char* xml =
R"(<?xml version="1.0" encoding="UTF-8"?>
<imppg>
    <lucy-richardson sigma="1.5000" iterations="50" deringing="false"/>
    <unsharp_mask_list>
        <unsharp_mask adaptive="false" sigma="1.0" amount_min="1.0000" amount_max="1.0" amount_threshold="0.0100" amount_width="0.1000"/>
    </unsharp_mask_list>
    <tone_curve smooth="true" is_gamma="false">0.0;0.0;0.0;0.0;</tone_curve>
    <normalization enabled="false" min="0.0" max="0.0"/>
</imppg>
    )";

    wxStringInputStream sIn(xml);
    const auto parsed = DeserializeSettings(sIn);
    BOOST_REQUIRE(!parsed.has_value());
}
