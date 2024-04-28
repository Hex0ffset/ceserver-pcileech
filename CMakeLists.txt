cmake_minimum_required(VERSION 3.25)

project(ceserver-pcileech-cpp LANGUAGES CXX)

include("cmake/definitions.cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ceserver-pcileech main.cpp)

if (WIN32)
    set(
            PLATFORM_SPECIFIC_FILES
            WinsockWrapper.cpp
            WinsockWrapper.h
    )
else ()
    set(
            PLATFORM_SPECIFIC_FILES
            LinuxSocketWrapper.cpp
            LinuxSocketWrapper.h
    )
endif ()

add_library(
        ceserver_lib
        STATIC
        CommandsToString.h
        Client.cpp
        Client.h
        ClientState.cpp
        ClientState.h
        Memory.cpp
        Memory.h
        Server.cpp
        Server.h
        SocketWrapper.h
        PCILeechWrapper.h
        ${PLATFORM_SPECIFIC_FILES}
)


if (WIN32)
    target_link_libraries(ceserver_lib PUBLIC wsock32 ws2_32)
endif ()

Include("cmake/functions.cmake")
copy_runtime_dlls(ceserver-pcileech)

Include("cmake/pcileech.cmake")
add_dependencies(ceserver_lib MemProcFS)
target_link_libraries(ceserver_lib PUBLIC vmm leechcore)
if (WIN32)
    add_dependencies(ceserver_lib FTD3XXBuild)
    target_link_libraries(ceserver_lib PUBLIC FTD3XX)
else ()
    add_dependencies(ceserver_lib LeechCore-plugins)
    target_link_libraries(ceserver_lib PUBLIC leechcore_ft601_driver_linux)
endif ()

target_include_directories(ceserver_lib PUBLIC "${CMAKE_BINARY_DIR}/memprocfs-install/includes")

target_link_libraries(ceserver-pcileech PRIVATE ceserver_lib)

include(FetchContent)

set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_Declare(
        absl
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp
        GIT_TAG master  # 'live at head' philosophy
)
set(ABSL_ENABLE_INSTALL OFF)
add_definitions(-DABSL_ENABLE_INSTALL=OFF)
FetchContent_MakeAvailable(absl)

set(
        ABSL_LIBS
        absl::log
        $<LINK_LIBRARY:WHOLE_ARCHIVE,absl::log_flags>
        absl::log_globals
        absl::log_initialize
        absl::log_severity
        absl::flags_parse
        absl::flags_usage
)

target_link_libraries(ceserver_lib PRIVATE ${ABSL_LIBS})
target_link_libraries(ceserver-pcileech PRIVATE ${ABSL_LIBS})
target_include_directories(ceserver_lib PUBLIC ${absl_SOURCE_DIR})
target_include_directories(ceserver-pcileech PUBLIC ${absl_SOURCE_DIR})

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest
        GIT_TAG main  # 'live at head' philosophy
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF)
set(INSTALL_GMOCK OFF)
add_definitions(-DINSTALL_GTEST=OFF -DINSTALL_GMOCK=OFF)
FetchContent_MakeAvailable(googletest)

# gives us FRIEND_TEST macro inside <gtest/gtest_prod.h>
target_include_directories(ceserver_lib PUBLIC ${googletest_SOURCE_DIR}/googletest/include)

include(CTest)
add_subdirectory(tests)

include(GNUInstallDirs)
list( APPEND CMAKE_INSTALL_RPATH "\$ORIGIN" )
list( APPEND CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX} )
list( APPEND CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib/ceserver-pcileech )
list( APPEND CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_LIBDIR}/ceserver-pcileech )
set_target_properties(
        ceserver-pcileech
        PROPERTIES
        INSTALL_RPATH "${CMAKE_INSTALL_RPATH}"
)
set_target_properties(
        ceserver_lib
        PROPERTIES
        INSTALL_RPATH "${CMAKE_INSTALL_RPATH}"
)

install(TARGETS ceserver-pcileech
        EXPORT ceserver-pcileech
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/ceserver-pcileech
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/ceserver-pcileech
)

if(UNIX)
    install(FILES
            ${CMAKE_BINARY_DIR}/leechcore-install/files/leechcore.so
            ${CMAKE_BINARY_DIR}/memprocfs-install/files/vmm.so
            ${CMAKE_BINARY_DIR}/leechcore-install/files/leechcore_ft601_driver_linux.so
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/ceserver-pcileech")
else()
    install(FILES
            ${CMAKE_BINARY_DIR}/leechcore-install/files/leechcore.dll
            ${CMAKE_BINARY_DIR}/memprocfs-install/files/vmm.dll
            ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/FTD3XX.dll
            DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()