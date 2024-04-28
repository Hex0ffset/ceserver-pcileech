include(ExternalProject)

ExternalProject_Add(
        CheatEngine
        GIT_REPOSITORY https://github.com/cheat-engine/cheat-engine
        GIT_TAG 7.5
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        BUILD_ALWAYS 0
)
ExternalProject_Get_property(CheatEngine SOURCE_DIR)
set(CE_SOURCE_DIR ${SOURCE_DIR})