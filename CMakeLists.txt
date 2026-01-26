cmake_minimum_required(VERSION 3.25...3.30)

# 1. SILENCE DEPRECATION WARNINGS (Must be before project())
set(CMAKE_POLICY_DEFAULT_CMP0000 NEW)

project(warp LANGUAGES CXX)

# 2. GLOBAL SETTINGS
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# 3. IPO/LTO SUPPORT CHECK
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

# 4. DEPENDENCIES (FetchContent)
include(FetchContent)
include(Dependencies.cmake)

FetchContent_Declare(spdlog  GIT_REPOSITORY ${SPDLOG_REPO}  GIT_TAG ${SPDLOG_VERSION})
FetchContent_Declare(glaze   GIT_REPOSITORY ${GLAZE_REPO}   GIT_TAG ${GLAZE_VERSION})
FetchContent_Declare(httplib GIT_REPOSITORY ${HTTPLIB_REPO} GIT_TAG ${HTTPLIB_VERSION})
FetchContent_Declare(croncpp GIT_REPOSITORY ${CRONCPP_REPO} GIT_TAG ${CRONCPP_VERSION})

set(SPDLOG_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(croncpp glaze httplib spdlog)

set(WARP_SOURCES
    include/warp/api/api-base.h
    include/warp/api/api-emby-types.h
    include/warp/api/api-emby.h
    include/warp/api/api-jellystat-types.h
    include/warp/api/api-jellystat.h
    include/warp/api/api-manager.h
    include/warp/api/api-plex-types.h
    include/warp/api/api-plex.h
    include/warp/api/api-response.h
    include/warp/api/api-tautulli-types.h
    include/warp/api/api-tautulli.h
    include/warp/api/api-types.h
    include/warp/log/log.h
    include/warp/log/logger.h
    include/warp/log/log-types.h
    include/warp/log/log-utils.h
    include/warp/scheduler/cron-scheduler.h
    include/warp/base.h
    include/warp/types.h
    include/warp/utils.h
    src/api/api-base.cpp
    src/api/api-emby-json-types.h
    src/api/api-emby.cpp
    src/api/api-jellystat-json-types.h
    src/api/api-jellystat.cpp
    src/api/api-manager.cpp
    src/api/api-plex-json-types.h
    src/api/api-plex.cpp
    src/api/api-tautulli-json-types.h
    src/api/api-tautulli.cpp
    src/api/api-utils.h
    src/logger/ansii-formatter.cpp
    src/logger/ansii-formatter.h
    src/logger/ansii-remove-formatter.cpp
    src/logger/ansii-remove-formatter.h
    src/logger/internal-types.h
    src/logger/log-apprise-sync.h
    src/logger/log-gotify-sync.h
    src/logger/logger.cpp
    src/scheduler/cron-scheduler.cpp
    src/base.cpp
)

# 6. CREATE THE TARGET (Only call this ONCE)
add_library(warp STATIC ${WARP_SOURCES})
add_library(warp::warp ALIAS warp)

# 7. APPLY TARGET PROPERTIES (Must be AFTER add_executable)
if(ipo_supported)
    set_property(TARGET warp PROPERTY 
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
    )
endif()

target_compile_options(warp PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /MP>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# 8. PLATFORM SPECIFIC
if(UNIX)
    target_link_options(warp PRIVATE 
        "$<$<CONFIG:Release>:-s>"
        -static-libstdc++ 
        -static-libgcc
    )
else()
    # Apply MSVC Link-Time Code Generation (The Windows version of LTO)
    target_link_options(warp PRIVATE $<$<CONFIG:Release>:/LTCG>)
endif()

# 9. PRECOMPILED HEADERS
target_precompile_headers(warp PRIVATE
    <format>
    <string>
    <string_view>
    <vector>
    <memory>
    <regex>

    # Third-Party
    <croncpp.h>
    <spdlog/spdlog.h>
    <glaze/glaze.hpp>
    <httplib.h>
)

# 10. INCLUDES & LINKING
target_include_directories(warp 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(warp 
    PRIVATE
        httplib::httplib
        croncpp::croncpp
        spdlog::spdlog
        glaze::glaze
)