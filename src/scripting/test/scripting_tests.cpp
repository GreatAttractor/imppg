#include "ScriptTestFixture.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <future>
#include <memory>

namespace fs = std::filesystem;

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

BOOST_FIXTURE_TEST_CASE(LoadImageAsMono32f, ScriptTestFixture)
{
    std::string script{R"(

image = imppg.load_image_as_mono32f("$ROOT/image.tif")
imppg.test.notify_image(image)

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    c_Image image{128, 64, PixelFormat::PIX_MONO8};
    image.ClearToZero();
    image.SaveToFile(root / "image.tif", OutputFormat::TIFF_16);

    RunScript(script.c_str());

    const auto& loadedImage = GetImageNotification();
    BOOST_CHECK_EQUAL(image.GetWidth(), loadedImage.GetWidth());
    BOOST_CHECK_EQUAL(image.GetHeight(), loadedImage.GetHeight());
    BOOST_CHECK(PixelFormat::PIX_MONO32F == loadedImage.GetPixelFormat());
}
