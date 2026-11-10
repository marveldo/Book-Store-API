set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.10)
project(cpp_server CXX)

# Custom file extension configuration
if(NOT DEFINED DEFAULT_SOURCE_EXTENSION)
    set(DEFAULT_SOURCE_EXTENSION "cc" CACHE STRING "Default source file extension (e.g., cc, cpp, cxx)")
endif()
if(NOT DEFINED DEFAULT_HEADER_EXTENSION)
    set(DEFAULT_HEADER_EXTENSION "h" CACHE STRING "Default header file extension (e.g., h, hpp, hxx)")
endif()

include(CheckIncludeFileCXX)

check_include_file_cxx(any HAS_ANY)
check_include_file_cxx(string_view HAS_STRING_VIEW)
check_include_file_cxx(coroutine HAS_COROUTINE)
if (NOT "${CMAKE_CXX_STANDARD}" STREQUAL "")
    # Do nothing
elseif (HAS_ANY AND HAS_STRING_VIEW AND HAS_COROUTINE)
    set(CMAKE_CXX_STANDARD 20)
elseif (HAS_ANY AND HAS_STRING_VIEW)
    set(CMAKE_CXX_STANDARD 17)
else ()
    set(CMAKE_CXX_STANDARD 14)
endif ()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} main.${DEFAULT_SOURCE_EXTENSION})

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon
# add_subdirectory(drogon)
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
# MSYS2 ships two Drogon configs: lib/cmake/Drogon (DLL) and lib/cmake/Drogon-static.
# The headers are built for the DLL (dllimport), so linking the static one gives
# "undefined reference to __imp_...". Prefer the DLL config when it exists.
if(MINGW)
    get_filename_component(_toolchain_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    if(EXISTS "${_toolchain_bin}/../lib/cmake/Drogon/DrogonConfig.cmake")
        set(Drogon_DIR "${_toolchain_bin}/../lib/cmake/Drogon" CACHE PATH "" FORCE)
    endif()
endif()

find_package(Drogon CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

# Only the static package exists: tell the headers not to use dllimport.
if(MINGW AND Drogon_DIR MATCHES "static")
    add_compile_definitions(DROGON_STATIC_DEFINE TRANTOR_STATIC_DEFINE)
endif()

# ##############################################################################

if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif (CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "use c++17")
else ()
    message(STATUS "use c++20")
endif ()

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(models MODEL_SRC)
aux_source_directory(settings SETTINGS_SRC)
aux_source_directory(utils UTILS_SRC)
aux_source_directory(bootstrap BOOTSTRAP_SRC)
aux_source_directory(middleware MIDDLEWARE_SRC)
aux_source_directory(events EVENTS_SRC)
aux_source_directory(queues QUEUES_SRC)
aux_source_directory(queues/handlers HANDLERS_SRC)
# To read every file in every folders of services 
file(GLOB_RECURSE SERVICES_SRC CONFIGURE_DEPENDS services/*.cpp services/*.cc)

# ---- Header-only libraries, downloaded by CMake (same version on Windows and in Docker) ----
include(FetchContent)

# nlohmann/json: the official release archive (much smaller than cloning the repo)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# jwt-cpp: signs/verifies tokens with OpenSSL (already installed, because Drogon needs it)
FetchContent_Declare(jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.2
    GIT_SHALLOW    TRUE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::SODIUM)

set(JWT_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)   # don't build its examples
set(JWT_DISABLE_PICOJSON ON  CACHE BOOL "" FORCE)   # we use nlohmann, not picojson

FetchContent_Declare(inja
    GIT_REPOSITORY https://github.com/pantor/inja.git
    GIT_TAG        v3.5.0
    GIT_SHALLOW    TRUE)

set(INJA_USE_EMBEDDED_JSON OFF CACHE BOOL "" FORCE)
set(INJA_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
set(INJA_INSTALL           OFF CACHE BOOL "" FORCE)
set(INJA_EXPORT            OFF CACHE BOOL "" FORCE)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json jwt-cpp inja)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json jwt-cpp::jwt-cpp pantor::inja)

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})
# use the following line to create views with namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE)
# use the following line to create views with namespace CHANGE_ME prefixed
# and path namespaces.
# drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
#                     ${CMAKE_CURRENT_BINARY_DIR} TRUE CHANGE_ME)

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/models)
target_sources(${PROJECT_NAME}
               PRIVATE
               ${SRC_DIR}
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${MODEL_SRC}
               ${SETTINGS_SRC}
               ${UTILS_SRC}
               ${SERVICES_SRC}
               ${BOOTSTRAP_SRC}
               ${MIDDLEWARE_SRC}
               ${EVENTS_SRC}
               ${QUEUES_SRC}
               ${HANDLERS_SRC}
               )
# ##############################################################################
# uncomment the following line for dynamically loading views
# set_property(TARGET ${PROJECT_NAME} PROPERTY ENABLE_EXPORTS ON)

# ##############################################################################

add_subdirectory(test)
