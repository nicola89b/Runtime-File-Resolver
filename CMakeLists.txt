cmake_minimum_required(VERSION 4.0)

project(
    RuntimeFileResolver
    VERSION 1.3.0
    LANGUAGES CXX
)

# ---- Options ----

set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)

# ---- Environment & Vcpkg ----

if (NOT DEFINED ENV{VCPKG_ROOT} AND DEFINED ENV{VCPKG_INSTALLATION_ROOT})
    set(ENV{VCPKG_ROOT} $ENV{VCPKG_INSTALLATION_ROOT})
endif()

if (DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# ---- Include Guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please create a separate build directory.")
endif()

# ---- Globals ----

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if (MSVC)
    add_compile_options(/MP)
    string(REGEX REPLACE "/Ob[0-3]" "/Ob3"
        CMAKE_CXX_FLAGS_RELEASE
        "${CMAKE_CXX_FLAGS_RELEASE}")
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

# ---- Dependencies ----

set(SKSE64_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/skse64")
set(COMMON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/common")
set(RFR_IERRORS_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/extern-overrides/IErrors.h")

find_package(nlohmann_json CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(common
    GIT_REPOSITORY https://github.com/ianpatt/common.git
    GIT_TAG 64e233c096735551f6ac9a773726a8a3960e46cd
    SOURCE_DIR "${COMMON_ROOT}"
)
FetchContent_Declare(skse64
    GIT_REPOSITORY https://github.com/ianpatt/skse64.git
    GIT_TAG 4cd2e34face74d5247fa38888c7542ad45d4a1d2
    SOURCE_DIR "${SKSE64_ROOT}"
    SOURCE_SUBDIR "_cmake_no_build"
)
FetchContent_MakeAvailable(common skse64)

if(EXISTS "${RFR_IERRORS_OVERRIDE}")
    file(COPY_FILE "${RFR_IERRORS_OVERRIDE}" "${COMMON_ROOT}/common/IErrors.h" ONLY_IF_DIFFERENT)
endif()

# ---- Targets ----

add_library(RuntimeFileResolverCore STATIC
    src/FileIO.cpp
    src/LoadOrder.cpp
    src/Resolver.cpp
    src/Rules.cpp
    src/RulesMode.cpp
    src/Template.cpp
)

target_compile_features(RuntimeFileResolverCore PRIVATE cxx_std_20)

target_compile_definitions(RuntimeFileResolverCore PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(RuntimeFileResolverCore PRIVATE /W4 /permissive- /EHsc)

target_include_directories(RuntimeFileResolverCore PUBLIC
    src
    "${COMMON_ROOT}"
)

target_link_libraries(RuntimeFileResolverCore PUBLIC
    nlohmann_json::nlohmann_json
    xse::common
)

add_library(
    ${PROJECT_NAME}
    SHARED
    src/main.cpp
    src/Runtime.cpp
    "${SKSE64_ROOT}/skse64_common/Relocation.cpp"
    "${SKSE64_ROOT}/skse64_common/Utilities.cpp"
)

set_source_files_properties(
    "${SKSE64_ROOT}/skse64_common/Utilities.cpp"
    PROPERTIES COMPILE_OPTIONS "/FI${COMMON_ROOT}/common/IPrefix.h;/wd4189"
)

target_compile_features(
    ${PROJECT_NAME}
    PRIVATE
        cxx_std_20
)

target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)

target_compile_options(
    ${PROJECT_NAME}
    PRIVATE
        /W4
        /permissive-
        /EHsc
        "$<$<CONFIG:RELEASE>:/Zc:inline;/JMC->"
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        src
        "${COMMON_ROOT}"
        "${SKSE64_ROOT}"
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        RuntimeFileResolverCore
        Shell32
)

target_link_options(
    ${PROJECT_NAME}
    PRIVATE
        "$<$<CONFIG:RELEASE>:/DEBUG:NONE;/INCREMENTAL:NO;/OPT:REF;/OPT:ICF>"
)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "RuntimeFileResolver"
)
