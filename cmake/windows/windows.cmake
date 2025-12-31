set(PLATFORM_SOURCES 3rdparty/WinCommander.cpp sys/windows/guihelper.cpp sys/windows/MiniDump.cpp)
set(PLATFORM_LIBRARIES wininet wsock32 ws2_32 user32 rasapi32 iphlpapi)

include(cmake/windows/generate_product_version.cmake)

set(_app_version "${APP_VERSION_STR}")
if (NOT _app_version OR "${_app_version}" STREQUAL "")
    set(_app_version "1.0.0")
endif ()
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ver_match "${_app_version}")
set(_ver_major "${CMAKE_MATCH_1}")
set(_ver_minor "${CMAKE_MATCH_2}")
set(_ver_patch "${CMAKE_MATCH_3}")
if (NOT _ver_major OR "${_ver_major}" STREQUAL "")
    set(_ver_major 1)
    set(_ver_minor 0)
    set(_ver_patch 0)
endif ()

generate_product_version(
        QV2RAY_RC
        ICON "${CMAKE_SOURCE_DIR}/res/cofebox.ico"
        NAME "${APP_DISPLAY_NAME}"
        BUNDLE "${APP_DISPLAY_NAME}"
        COMPANY_NAME "${APP_DISPLAY_NAME}"
        COMPANY_COPYRIGHT "${APP_DISPLAY_NAME}"
        FILE_DESCRIPTION "${APP_DISPLAY_NAME}"
        VERSION_MAJOR ${_ver_major}
        VERSION_MINOR ${_ver_minor}
        VERSION_PATCH ${_ver_patch}
        VERSION_REVISION 0
)
add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
set(GUI_TYPE WIN32)
if (MINGW)
    if (NOT DEFINED MinGW_ROOT)
        set(MinGW_ROOT "C:/msys64/mingw64")
    endif ()
else ()
    add_compile_options("/utf-8")
    add_compile_options("/std:c++17")
    add_definitions(-D_WIN32_WINNT=0x600 -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)
endif ()
