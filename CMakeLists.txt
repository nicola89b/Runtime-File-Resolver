cmake_minimum_required(VERSION 4.0)

set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
project(RuntimeFileResolver VERSION 1.0 LANGUAGES CXX)


set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(SKSE64_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/skse64")
set(COMMON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/extern/common")
set(RFR_IERRORS_OVERRIDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/extern-overrides/IErrors.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RFR_IERRORS_OVERRIDE}")

find_package(nlohmann_json CONFIG REQUIRED)
include(FetchContent)

FetchContent_Declare(
    common
    GIT_REPOSITORY https://github.com/ianpatt/common.git
    GIT_TAG 64e233c096735551f6ac9a773726a8a3960e46cd
    SOURCE_DIR "${COMMON_ROOT}"
)

FetchContent_Declare(
    skse64
    GIT_REPOSITORY https://github.com/ianpatt/skse64.git
    GIT_TAG 4cd2e34face74d5247fa38888c7542ad45d4a1d2
    SOURCE_DIR "${SKSE64_ROOT}"
    SOURCE_SUBDIR "_cmake_no_build"
)

set(FETCHCONTENT_LOCK_FILE "${CMAKE_BINARY_DIR}/fetchcontent.lock")
file(LOCK "${FETCHCONTENT_LOCK_FILE}" GUARD PROCESS TIMEOUT 300)
set(CMAKE_SKIP_INSTALL_RULES TRUE)

if(EXISTS "${COMMON_ROOT}/CMakeLists.txt" AND EXISTS "${COMMON_ROOT}/common")
    set(FETCHCONTENT_SOURCE_DIR_COMMON "${COMMON_ROOT}")
endif()

if(EXISTS "${SKSE64_ROOT}/skse64_common/BranchTrampoline.cpp")
    set(FETCHCONTENT_SOURCE_DIR_SKSE64 "${SKSE64_ROOT}")
endif()

FetchContent_MakeAvailable(common skse64)

file(COPY_FILE "${RFR_IERRORS_OVERRIDE}" "${COMMON_ROOT}/common/IErrors.h" ONLY_IF_DIFFERENT)

file(LOCK "${FETCHCONTENT_LOCK_FILE}" RELEASE)


add_library(RuntimeFileResolverCore STATIC
    src/FileIO.cpp
    src/LoadOrder.cpp
    src/Resolver.cpp
    src/Rules.cpp
    src/RulesMode.cpp
    src/Template.cpp
)

target_include_directories(RuntimeFileResolverCore PUBLIC
    src
    "${COMMON_ROOT}"
)

target_link_libraries(RuntimeFileResolverCore PUBLIC
    nlohmann_json::nlohmann_json
    xse::common
)

target_compile_features(RuntimeFileResolverCore PUBLIC cxx_std_20)
target_compile_definitions(RuntimeFileResolverCore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(RuntimeFileResolverCore PRIVATE /W4 /permissive- /EHsc)
set_target_properties(RuntimeFileResolverCore PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)


add_library(RuntimeFileResolver SHARED
    src/main.cpp
    src/Runtime.cpp
    "${SKSE64_ROOT}/skse64_common/Relocation.cpp"
    "${SKSE64_ROOT}/skse64_common/Utilities.cpp"
)

set_source_files_properties(
    "${SKSE64_ROOT}/skse64_common/Utilities.cpp"
    PROPERTIES COMPILE_OPTIONS "/FI${COMMON_ROOT}/common/IPrefix.h;/wd4189"
)

target_include_directories(RuntimeFileResolver PRIVATE
    src
    "${COMMON_ROOT}"
    "${SKSE64_ROOT}"
)

target_link_libraries(RuntimeFileResolver PRIVATE RuntimeFileResolverCore Shell32)
target_compile_features(RuntimeFileResolver PRIVATE cxx_std_20)
target_compile_definitions(RuntimeFileResolver PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(RuntimeFileResolver PRIVATE /W4 /permissive- /EHsc)
target_link_options(RuntimeFileResolver PRIVATE
    "$<$<CONFIG:Release>:/DEBUG:NONE>"
    "$<$<CONFIG:Release>:/INCREMENTAL:NO>"
)

set_target_properties(RuntimeFileResolver PROPERTIES
    PREFIX ""
    OUTPUT_NAME "RuntimeFileResolver"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)
