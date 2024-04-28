cmake_minimum_required(VERSION 3.25)

include("../cmake/definitions.cmake")

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "*.cpp")

if (UNIX)
    list(FILTER sources EXCLUDE REGEX WinsockWrapper)
else ()
    list(FILTER sources EXCLUDE REGEX LinuxSocketWrapper)
endif ()

add_executable(tests ${sources})

target_link_libraries(tests ceserver_lib)

target_link_libraries(tests ${ABSL_LIBS})
target_include_directories(tests PUBLIC ${absl_SOURCE_DIR})


target_link_libraries(
        tests
        GTest::gtest_main
        GTest::gmock_main
)

Include("../cmake/functions.cmake")
copy_runtime_dlls(tests)

include(GoogleTest)
gtest_discover_tests(tests)