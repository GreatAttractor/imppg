#include "ScriptTestFixture.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>
#include <boost/test/unit_test.hpp>
#include <filesystem>

namespace fs = std::filesystem;

BOOST_FIXTURE_TEST_CASE(ListFilesFilteredByExtension, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/*.tif") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateEmptyFile(root / "file1.tif");
    CreateEmptyFile(root / "file2.tif");
    CreateEmptyFile(root / "file3.zip");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "file1.tif").generic_string(),
        (root / "file2.tif").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListAllFiles, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/*") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateEmptyFile(root / "file1.tif");
    CreateEmptyFile(root / "file2");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "file1.tif").generic_string(),
        (root / "file2").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListFilesInDirectory, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/subdir") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    fs::create_directory(root / "subdir");
    CreateEmptyFile(root / "subdir" / "file1");
    CreateEmptyFile(root / "subdir" / "file2");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "subdir" / "file1").generic_string(),
        (root / "subdir" / "file2").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListFilesByNameMask, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/file*suffix.tif") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateEmptyFile(root / "file1_suffix.tif");
    CreateEmptyFile(root / "file20suffix.tif");
    CreateEmptyFile(root / "file300suffix");
    CreateEmptyFile(root / "buzz");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "file1_suffix.tif").generic_string(),
        (root / "file20suffix.tif").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(NonExistentDirectory_ListFiles_ExecutionFails, ScriptTestFixture)
{
    std::string script{R"(

imppg.filesystem.list_files("/985849872632452/non-existent-dir")

    )"};

    BOOST_CHECK(!RunScript(script.c_str()));
}

BOOST_FIXTURE_TEST_CASE(ListFilesSortedFilteredByExtension, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files_sorted("$ROOT/*.tif") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateEmptyFile(root / "file1.tif");
    CreateEmptyFile(root / "file2.tif");
    CreateEmptyFile(root / "file3.zip");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "file1.tif").generic_string(),
        (root / "file2.tif").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListAllFilesSorted, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files_sorted("$ROOT/*") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateEmptyFile(root / "file1.tif");
    CreateEmptyFile(root / "file2");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "file1.tif").generic_string(),
        (root / "file2").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListFilesSortedInDirectory, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files_sorted("$ROOT/subdir") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    fs::create_directory(root / "subdir");
    CreateEmptyFile(root / "subdir" / "file1");
    CreateEmptyFile(root / "subdir" / "file2");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "subdir" / "file1").generic_string(),
        (root / "subdir" / "file2").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListFilesSortedByNameMask, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files_sorted("$ROOT/file*suffix.tif") do
    imppg.test.notify_string_unordered(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateEmptyFile(root / "file1_suffix.tif");
    CreateEmptyFile(root / "file20suffix.tif");
    CreateEmptyFile(root / "file300suffix");
    CreateEmptyFile(root / "buzz");

    BOOST_REQUIRE(RunScript(script.c_str()));

    CheckUnorderedStringNotifications({
        (root / "file1_suffix.tif").generic_string(),
        (root / "file20suffix.tif").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(NonExistentDirectory_ListFilesSorted_ExecutionFails, ScriptTestFixture)
{
    std::string script{R"(

imppg.filesystem.list_files_sorted("/985849872632452/non-existent-dir")

    )"};

    BOOST_CHECK(!RunScript(script.c_str()));
}

BOOST_FIXTURE_TEST_CASE(ListFilesSorted_ResultsSorted, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files_sorted("$ROOT/*.tif") do
    imppg.test.notify_string(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    for (int i = 0; i < 100; ++i)
    {
        CreateEmptyFile(root / boost::str(boost::format("file1_%05d.tif") % i));
    }

    BOOST_REQUIRE(RunScript(script.c_str()));

    const auto& strings = GetStringNotifications();
    for (std::size_t i = 1; i < strings.size(); ++i)
    {
        BOOST_CHECK(strings[i - 1] <= strings[i]);
    }
}
