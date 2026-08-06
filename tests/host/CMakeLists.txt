cmake_minimum_required(VERSION 3.16)
project(host_tests C)

set(CMAKE_C_STANDARD 11)

# HOST_BUILD disables all ESP-IDF/FreeRTOS headers in guarded source files
add_compile_definitions(HOST_BUILD=1)

include(FetchContent)

# --- Unity test framework (DOWNLOAD_ONLY avoids cmake version compatibility issues) ---
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
    DOWNLOAD_ONLY  TRUE
)
FetchContent_Populate(unity)

add_library(unity STATIC
    ${unity_SOURCE_DIR}/src/unity.c
)
target_include_directories(unity PUBLIC ${unity_SOURCE_DIR}/src)

# --- cJSON (DOWNLOAD_ONLY avoids cmake version compatibility issues) ---
FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.18
    GIT_SHALLOW    TRUE
    DOWNLOAD_ONLY  TRUE
)
FetchContent_Populate(cjson)

add_library(cjson STATIC
    ${cjson_SOURCE_DIR}/cJSON.c
)
target_include_directories(cjson PUBLIC ${cjson_SOURCE_DIR})
target_compile_definitions(cjson PRIVATE CJSON_HIDE_SYMBOLS)

# --- Sources under test ---
set(SOURCES_UNDER_TEST
    ../../components/weather_service/weather_parser.c
    ../../components/time_sync/time_format.c
    ../../components/app_config/app_config.c
    ../../main/status_builder.c
)

# --- Test sources ---
set(TEST_SOURCES
    test_main.c
    test_weather_parser.c
    test_time_format.c
    test_app_config.c
    test_status_builder.c
)

add_executable(host_tests ${SOURCES_UNDER_TEST} ${TEST_SOURCES})

target_include_directories(host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/app_config/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/app_state/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/weather_service/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/time_sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../main
)

target_link_libraries(host_tests unity cjson)

