#pragma once

#include <filesystem>
#include <lua.hpp>
#include <optional>
#include <string>
#include <regex>

namespace scripting
{

class DirectoryIterator
{
public:
    DirectoryIterator(std::string fileNamePattern);

    std::optional<std::string> Next();

private:
    std::filesystem::directory_iterator m_DirIter;

    std::regex m_RegEx;
};

}
