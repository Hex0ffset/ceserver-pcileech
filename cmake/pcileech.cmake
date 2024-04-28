include(ExternalProject)

if (WIN32)
    set(PCILEECH_BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} /p:Platform=${CMAKE_GENERATOR_PLATFORM})
else()
    set(PCILEECH_BUILD_COMMAND $(MAKE))
endif()

message("Using build command ${PCILEECH_BUILD_COMMAND}")

ExternalProject_Add(
        LeechCore
        GIT_REPOSITORY https://github.com/ufrisk/LeechCore
        GIT_TAG v2.18
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${PCILEECH_BUILD_COMMAND}
        SOURCE_SUBDIR leechcore
        BUILD_IN_SOURCE TRUE
        INSTALL_COMMAND ""
        BUILD_ALWAYS 0
)
ExternalProject_Add_Step(LeechCore postbuild
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/install-deps.cmake" "<SOURCE_DIR>/leechcore" "${CMAKE_BINARY_DIR}/leechcore-install"
        DEPENDEES build
        DEPENDERS install
        ALWAYS 1
        USES_TERMINAL 1
)

if (UNIX)
    ExternalProject_Add(
            LeechCore-plugins
            GIT_REPOSITORY https://github.com/ufrisk/LeechCore-plugins
            GIT_TAG v2.0
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ${PCILEECH_BUILD_COMMAND}
            DEPENDS LeechCore
            SOURCE_SUBDIR leechcore_ft601_driver_linux
            BUILD_IN_SOURCE TRUE
            INSTALL_COMMAND ""
            BUILD_ALWAYS 0
    )
    ExternalProject_Add_Step(LeechCore-plugins prebuild
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/copy-deps.cmake" "${CMAKE_BINARY_DIR}/leechcore-install" "<SOURCE_DIR>"
            DEPENDEES configure
            DEPENDERS build
            ALWAYS 1
            USES_TERMINAL 1
    )
    ExternalProject_Add_Step(LeechCore-plugins postbuild
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/install-deps.cmake" "<SOURCE_DIR>/leechcore_ft601_driver_linux" "${CMAKE_BINARY_DIR}/leechcore-install"
            DEPENDEES build
            DEPENDERS install
            ALWAYS 1
            USES_TERMINAL 1
    )
endif()

ExternalProject_Add(
        MemProcFS
        GIT_REPOSITORY https://github.com/ufrisk/MemProcFS
        GIT_TAG v5.9
        PATCH_COMMAND git reset --hard
              COMMAND git apply "${CMAKE_CURRENT_LIST_DIR}/patches/memprocfs.patch"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${PCILEECH_BUILD_COMMAND}
        DEPENDS LeechCore
        SOURCE_SUBDIR vmm
        BUILD_IN_SOURCE TRUE
        INSTALL_COMMAND ""
        BUILD_ALWAYS 0
)
ExternalProject_Add_Step(MemProcFS prebuild
        COMMAND ${CMAKE_COMMAND} -E copy_directory "<SOURCE_DIR>/includes" "<SOURCE_DIR>/vmm/includes"
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/copy-deps.cmake" "${CMAKE_BINARY_DIR}/leechcore-install" "<SOURCE_DIR>/vmm"
        DEPENDEES configure
        DEPENDERS build
        ALWAYS 1
        USES_TERMINAL 1
)
ExternalProject_Add_Step(MemProcFS postbuild
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/install-deps.cmake" "<SOURCE_DIR>/vmm" "${CMAKE_BINARY_DIR}/memprocfs-install"
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/memprocfs-install/files ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech
        DEPENDEES build
        DEPENDERS install
        ALWAYS 1
        USES_TERMINAL 1
)

#[[ExternalProject_Add(
        PCILeech
        GIT_REPOSITORY https://github.com/ufrisk/pcileech
        GIT_TAG v4.17
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${PCILEECH_BUILD_COMMAND}
        DEPENDS LeechCore MemProcFS Dokany
        BUILD_IN_SOURCE TRUE
        SOURCE_SUBDIR pcileech
        INSTALL_COMMAND ""
        INSTALL_DIR "${CMAKE_BINARY_DIR}/pcileech-install"
)
ExternalProject_Add_Step(PCILeech prebuild
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/copy-deps.cmake" "${CMAKE_BINARY_DIR}/leechcore-install" "<SOURCE_DIR>/pcileech"
        DEPENDEES configure
        DEPENDERS build
        ALWAYS 1
        USES_TERMINAL 1
)
ExternalProject_Add_Step(PCILeech postbuild
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/install-deps.cmake" "<SOURCE_DIR>/pcileech" "${CMAKE_BINARY_DIR}/pcileech-install"
        DEPENDEES build
        DEPENDERS install
        ALWAYS 1
        USES_TERMINAL 1
)]]

ExternalProject_Add(
        FTD3XXBuild
        URL               "https://ftdichip.com/wp-content/uploads/2024/01/FTD3XXLibrary_v1.3.0.9.zip"
        URL_HASH          SHA256=e01f1145d7e9250bd2b113b2913ba513031df0b3b8d52b259de506a25acdc2bb
        CONFIGURE_COMMAND ""
        INSTALL_COMMAND   ""

        INSTALL_DIR       "${CMAKE_BINARY_DIR}/lib/ceserver-pcileech"

        BUILD_COMMAND     ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/Static_Lib/FTD3XX.lib <INSTALL_DIR>/FTD3XX.lib
        COMMAND           ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/x64/DLL/FTD3XX.dll <INSTALL_DIR>/FTD3XX.dll

        BUILD_ALWAYS 0
)

#link_directories("${CMAKE_BINARY_DIR}/leechcore-install/files")
#link_directories("${CMAKE_BINARY_DIR}/memprocfs-install/files")
#link_directories("${CMAKE_BINARY_DIR}/FTD3XX-install")
#
add_library(leechcore SHARED IMPORTED)
add_library(vmm SHARED IMPORTED)

if (WIN32)
    set_target_properties(leechcore PROPERTIES
            IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/leechcore.dll
            IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/leechcore.lib
            BUILD_WITH_INSTALL_RPATH TRUE
    )
    set_target_properties(vmm PROPERTIES
            IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/vmm.dll
            IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/vmm.lib
    )

    add_library(FTD3XX SHARED IMPORTED)
    set_target_properties(FTD3XX PROPERTIES
            IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/FTD3XX.dll
            IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/FTD3XX.lib
    )
else()
    set_target_properties(leechcore PROPERTIES
            IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/leechcore.so
            IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/leechcore.lib
    )
    set_target_properties(vmm PROPERTIES
            IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/vmm.so
            IMPORTED_IMPLIB ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/vmm.lib
    )

    add_library(leechcore_ft601_driver_linux SHARED IMPORTED)
    set_target_properties(leechcore_ft601_driver_linux PROPERTIES
            IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/ceserver-pcileech/leechcore_ft601_driver_linux.so
    )
endif()