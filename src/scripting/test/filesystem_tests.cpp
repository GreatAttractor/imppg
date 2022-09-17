#include "ScriptTestFixture.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>
#include <filesystem>

namespace fs = std::filesystem;

BOOST_FIXTURE_TEST_CASE(ListFilesFilteredByExtension, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/*.tif") do
    imppg.test.notify_string(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateFile(root / "file1.tif");
    CreateFile(root / "file2.tif");
    CreateFile(root / "file3.zip");

    RunScript(script.c_str());

    CheckStringNotifications({
        (root / "file1.tif").generic_string(),
        (root / "file2.tif").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListAllFiles, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/*") do
    imppg.test.notify_string(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateFile(root / "file1.tif");
    CreateFile(root / "file2");

    RunScript(script.c_str());

    CheckStringNotifications({
        (root / "file1.tif").generic_string(),
        (root / "file2").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListFilesInDirectory, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/subdir") do
    imppg.test.notify_string(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    fs::create_directory(root / "subdir");
    CreateFile(root / "subdir" / "file1");
    CreateFile(root / "subdir" / "file2");

    RunScript(script.c_str());

    CheckStringNotifications({
        (root / "subdir" / "file1").generic_string(),
        (root / "subdir" / "file2").generic_string()
    });
}

BOOST_FIXTURE_TEST_CASE(ListFilesByNameMask, ScriptTestFixture)
{
    std::string script{R"(

for f in imppg.filesystem.list_files("$ROOT/file*suffix.tif") do
    imppg.test.notify_string(f)
end

    )"};
    const auto root = fs::temp_directory_path();
    boost::algorithm::replace_all(script, "$ROOT", root.generic_string());
    CreateFile(root / "file1_suffix.tif");
    CreateFile(root / "file20suffix.tif");
    CreateFile(root / "file300suffix");
    CreateFile(root / "buzz");

    RunScript(script.c_str());

    CheckStringNotifications({
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