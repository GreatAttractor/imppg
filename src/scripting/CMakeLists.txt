add_library(scripting STATIC
    src/interop/classes/DirectoryIterator.cpp
    src/interop/classes/DirectoryIterator.h
    src/interop/classes/DummyObject1.cpp
    src/interop/classes/DummyObject1.h
    src/interop/classes/DummyObject2.cpp
    src/interop/classes/DummyObject2.h
    src/interop/classes/SettingsWrapper.cpp
    src/interop/classes/SettingsWrapper.h
    src/interop/functions/common.cpp
    src/interop/functions/common.h
    src/interop/functions/imppg_filesystem.cpp
    src/interop/functions/imppg_filesystem.h
    src/interop/functions/imppg_test.cpp
    src/interop/functions/imppg_test.h
    src/interop/functions/imppg.cpp
    src/interop/functions/imppg.h
    src/interop/interop.cpp
    src/interop/state.cpp
    src/interop/state.h
    src/script_runner.cpp
)

target_include_directories(scripting PUBLIC include)
target_include_directories(scripting PRIVATE src)


pkg_check_modules(LUA lua REQUIRED)
target_include_directories(scripting PRIVATE ${LUA_INCLUDE_DIRS})
#TODO check which minimal version of Lua we need and enforce it here (initial implementation uses 5.4)

target_link_libraries(scripting PRIVATE ${wxWidgets_LIBRARIES} ${LUA_LIBRARIES} common)

add_subdirectory(test)
