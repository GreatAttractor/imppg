#include "ScriptTestFixture.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>

namespace fs = std::filesystem;

// private definitions
namespace
{

template<typename T>
bool CheckAllPixelValues(const c_Image& image, T value)
{
    for (unsigned y = 0; y < image.GetHeight(); ++y)
    {
        const T* row = image.GetRowAs<T>(y);
        for (unsigned x = 0; x < image.GetWidth(); ++x)
        {
            if (row[x] != value) { return false; }
        }
    }

    return true;
}

}

BOOST_FIXTURE_TEST_CASE(CallModuleAndSubmoduleFunctions, ScriptTestFixture)
{
    const char* script = R"(

imppg.test.notify_string_unordered("s1")
imppg.test.test.notify_string_unordered("s2")

    )";

    BOOST_REQUIRE(RunScript(script));
    CheckUnorderedStringNotifications({"s1", "s2"});
}

BOOST_FIXTURE_TEST_CASE(LoadAndCheckImage, ScriptTestFixture)
{
    std::string script{R"(

image = imppg.load_image("$ROOT/image.tif")
imppg.test.notify_image(image)

    )"};
    const auto root = GetTestRoot();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    c_Image image{128, 64, PixelFormat::PIX_MONO8};
    image.ClearToZero();
    image.SaveToFile((root / "image.tif").string(), OutputFormat::TIFF_16);

    BOOST_REQUIRE(RunScript(script.c_str()));

    fs::remove(root / "image.tif");

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
processed_image:save("$ROOT/output.tif", imppg.TIFF_16)

    )"};
    const auto root = GetTestRoot();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    c_Image image{128, 64, PixelFormat::PIX_MONO8};
    image.ClearToZero();
    image.SaveToFile((root / "image.bmp").string(), OutputFormat::BMP_8);

    BOOST_REQUIRE(RunScript(script.c_str()));

    fs::remove(root / "image.bmp");

    const auto& processedImg = GetImageNotification();
    BOOST_REQUIRE(PixelFormat::PIX_MONO32F == processedImg.GetPixelFormat());
    BOOST_CHECK(CheckAllPixelValues(processedImg, 0.5f));

    const auto loadedProcessedImg = LoadImage((root / "output.tif").string());
    BOOST_REQUIRE(loadedProcessedImg.value().GetPixelFormat() == PixelFormat::PIX_MONO16);
    BOOST_CHECK(CheckAllPixelValues<std::uint16_t>(loadedProcessedImg.value(), 0xFFFF / 2));
}

BOOST_FIXTURE_TEST_CASE(ProcessImageFile, ScriptTestFixture)
{
    std::string script{R"(

imppg.process_image_file("$ROOT/image.bmp", "$ROOT/settings.xml", "$ROOT/output.tif", imppg.TIFF_16)

    )"};

    const auto root = GetTestRoot();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    c_Image image{128, 64, PixelFormat::PIX_MONO8};
    image.ClearToZero();
    image.SaveToFile((root / "image.bmp").string(), OutputFormat::BMP_8);

    ProcessingSettings settings{};
    // horizontal tone curve mapping everything to 0.5 brightness
    settings.toneCurve.UpdatePoint(0, 0.0, 0.5);
    settings.toneCurve.UpdatePoint(1, 1.0, 0.5);
    SaveSettings((root / "settings.xml").string(), settings);

    BOOST_REQUIRE(RunScript(script.c_str()));

    fs::remove(root / "image.bmp");

    auto processedImg = LoadImage((root / "output.tif").string()).value();
    fs::remove(root / "output.tif");
    BOOST_REQUIRE(PixelFormat::PIX_MONO16 == processedImg.GetPixelFormat());
    BOOST_CHECK(CheckAllPixelValues<std::uint16_t>(processedImg, 0xFFFF / 2));
}

BOOST_FIXTURE_TEST_CASE(MultiplyImagePixels, ScriptTestFixture)
{
    std::string script{R"(

image = imppg.load_image("$ROOT/image.tif")
image:multiply(0.5)
imppg.test.notify_image(image)

    )"};
    const auto root = GetTestRoot();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());

    constexpr unsigned WIDTH = 128;
    constexpr unsigned HEIGHT = 64;
    c_Image image{WIDTH, HEIGHT, PixelFormat::PIX_MONO16};

    for (unsigned y = 0; y < HEIGHT; ++y)
    {
        auto* row = image.GetRowAs<std::uint16_t>(y);
        for (unsigned x = 0; x < WIDTH; ++x)
        {
            row[x] = 0xFFFF;
        }
    }

    image.SaveToFile((root / "image.tif").string(), OutputFormat::TIFF_16);

    BOOST_REQUIRE(RunScript(script.c_str()));

    fs::remove(root / "image.tif");

    const auto& multiplied = GetImageNotification();
    BOOST_CHECK_EQUAL(WIDTH, multiplied.GetWidth());
    BOOST_CHECK_EQUAL(HEIGHT, multiplied.GetHeight());
    // image is converted to 32-bit floating-point upon loading
    BOOST_CHECK(PixelFormat::PIX_MONO32F == multiplied.GetPixelFormat());

    for (unsigned y = 0; y < HEIGHT; ++y)
    {
        const auto* row = multiplied.GetRowAs<float>(y);
        for (unsigned x = 0; x < WIDTH; ++x)
        {
            BOOST_REQUIRE_CLOSE(0.5, row[x], 1.0e-4);
        }
    }
}
