#include "ScriptTestFixture.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdint>
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

    BOOST_REQUIRE(RunScript(script));
    CheckStringNotifications({"s1", "s2"});
}

BOOST_FIXTURE_TEST_CASE(LoadImage, ScriptTestFixture)
{
    std::string script{R"(

image = imppg.load_image("$ROOT/image.tif")
imppg.test.notify_image(image)

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    c_Image image{128, 64, PixelFormat::PIX_MONO8};
    image.ClearToZero();
    image.SaveToFile(root / "image.tif", OutputFormat::TIFF_16);

    BOOST_REQUIRE(RunScript(script.c_str()));

    const auto& loadedImage = GetImageNotification();
    BOOST_CHECK_EQUAL(image.GetWidth(), loadedImage.GetWidth());
    BOOST_CHECK_EQUAL(image.GetHeight(), loadedImage.GetHeight());
    // image is converted to 32-bit floating-point upon loading
    BOOST_CHECK(PixelFormat::PIX_MONO32F == loadedImage.GetPixelFormat());
}

BOOST_FIXTURE_TEST_CASE(LoadAndProcessImage, ScriptTestFixture)
{
    std::string script{R"(

settings = imppg.new_settings()
-- horizontal tone curve mapping everything to 0.5 brightness
settings:tc_set_point(0, 0.0, 0.5)
settings:tc_set_point(1, 1.0, 0.5)
image = imppg.load_image("$ROOT/image.bmp")

processed_image = imppg.process_image(image, settings)

imppg.test.notify_image(processed_image)

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    c_Image image{128, 64, PixelFormat::PIX_MONO8};
    image.ClearToZero();
    image.SaveToFile(root / "image.bmp", OutputFormat::BMP_8);

    BOOST_REQUIRE(RunScript(script.c_str()));

    const auto& processedImg = GetImageNotification();
    BOOST_REQUIRE(PixelFormat::PIX_MONO32F == processedImg.GetPixelFormat());
    for (unsigned y = 0; y < processedImg.GetHeight(); ++y)
    {
        const float* row = processedImg.GetRowAs<float>(y);
        for (unsigned x = 0; x < processedImg.GetWidth(); ++x)
        {
            BOOST_REQUIRE(0.5 == row[x]);
        }
    }
}

//TODO: need another param: output format to save
// BOOST_FIXTURE_TEST_CASE(ProcessImageFile, ScriptTestFixture)
// {
//     std::string script{R"(

// imppg.process_image_file("$ROOT/image.bmp", "$ROOT/settings.xml", "$ROOT/output.bmp")

//     )"};
//     const auto root = fs::temp_directory_path();
//     boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

//     c_Image image{128, 64, PixelFormat::PIX_MONO8};
//     image.ClearToZero();
//     image.SaveToFile(root / "image.bmp", OutputFormat::BMP_8);

//     ProcessingSettings settings{};
//     // horizontal tone curve mapping everything to 0.5 brightness
//     settings.toneCurve.UpdatePoint(0, 0.0, 0.5);
//     settings.toneCurve.UpdatePoint(1, 1.0, 0.5);
//     SaveSettings(root / "settings.xml", settings);

//     BOOST_REQUIRE(RunScript(script.c_str()));

//     auto processedImg = LoadImageAs
//     BOOST_REQUIRE(PixelFormat::PIX_MONO32F == processedImg.GetPixelFormat());
//     for (unsigned y = 0; y < processedImg.GetHeight(); ++y)
//     {
//         const float* row = processedImg.GetRowAs<float>(y);
//         for (unsigned x = 0; x < processedImg.GetWidth(); ++x)
//         {
//             BOOST_REQUIRE(0.5 == row[x]);
//         }
//     }
// }
