# Release
file(STRINGS nekoray_version.txt NKR_VERSION)
add_compile_definitions(NKR_VERSION=\"${NKR_VERSION}\")

set(APP_DISPLAY_NAME "CofeBox")
set(APP_ID "cofebox")
set(APP_CONFIG_ID "nekoray")

if(DEFINED ENV{APP_VERSION_STR})
    set(APP_VERSION_STR "$ENV{APP_VERSION_STR}")
elseif(DEFINED APP_VERSION_STR AND NOT "${APP_VERSION_STR}" STREQUAL "")
    set(APP_VERSION_STR "${APP_VERSION_STR}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" APP_VERSION_STR LIMIT_COUNT 1)
else()
    execute_process(
        COMMAND git describe --tags --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE APP_VERSION_STR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(NOT APP_VERSION_STR OR "${APP_VERSION_STR}" STREQUAL "")
    set(APP_VERSION_STR "${NKR_VERSION}")
endif()

string(REGEX REPLACE "^[Vv]" "" APP_VERSION_STR "${APP_VERSION_STR}")

add_compile_definitions(APP_DISPLAY_NAME=\"${APP_DISPLAY_NAME}\")
add_compile_definitions(APP_ID=\"${APP_ID}\")
add_compile_definitions(APP_CONFIG_ID=\"${APP_CONFIG_ID}\")
add_compile_definitions(APP_VERSION_STR=\"${APP_VERSION_STR}\")

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNKR_CPP_DEBUG")

# Func
function(nkr_add_compile_definitions arg)
    message("[add_compile_definitions] ${ARGV}")
    add_compile_definitions(${ARGV})
endfunction()
