add_library(scripting STATIC
    src/interop/classes/DirectoryIterator.cpp
    src/interop/classes/DirectoryIterator.h
    src/interop/classes/DummyObject1.cpp
    src/interop/classes/DummyObject1.h
    src/interop/classes/DummyObject2.cpp
    src/interop/classes/DummyObject2.h
    src/interop/classes/SettingsWrapper.cpp
    src/interop/classes/SettingsWrapper.h
    src/interop/classes/ImageWrapper.cpp
    src/interop/classes/ImageWrapper.h
    src/interop/modules/common.cpp
    src/interop/modules/common.h
    src/interop/modules/imppg_filesystem.cpp
    src/interop/modules/imppg_filesystem.h
    src/interop/modules/imppg_test.cpp
    src/interop/modules/imppg_test.h
    src/interop/modules/imppg.cpp
    src/interop/modules/imppg.h
    src/interop/interop.cpp
    src/interop/state.cpp
    src/interop/state.h
    src/script_image_processor.cpp
    src/script_runner.cpp
)

include(../../utils.cmake)
set_compiler_options(scripting)

target_include_directories(scripting PUBLIC include)
target_include_directories(scripting PRIVATE src ${Boost_INCLUDE_DIRS})


pkg_check_modules(LUA lua REQUIRED)
target_include_directories(scripting PRIVATE ${LUA_INCLUDE_DIRS})
#TODO check which minimal version of Lua we need and enforce it here (initial implementation uses 5.4)

target_link_libraries(scripting PRIVATE ${wxWidgets_LIBRARIES} ${LUA_LIBRARIES} backend common)

add_subdirectory(test)
