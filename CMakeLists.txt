cmake_minimum_required(VERSION 3.16)

project(woke
  VERSION 0.2.0
  DESCRIPTION "Cross-platform library to keep a desktop awake and responsive"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(WOKE_BUILD_TESTS    "Build the woke test suite"  ON)
option(WOKE_BUILD_EXAMPLES "Build the woke example app" ON)

# ---------------------------------------------------------------------------
# Select the platform backends (one sleep + one nap backend per platform)
# ---------------------------------------------------------------------------
set(_woke_platform_sources)
if(WIN32)
  list(APPEND _woke_platform_sources
    src/backend_sleep_windows.cpp
    src/backend_nap_windows.cpp)
elseif(APPLE)
  # The nap backend is Objective-C++ (NSProcessInfo). Deliberately do NOT
  # enable_language(OBJCXX) for it: enabled languages are global to the whole
  # build, but the OBJCXX rule variables would exist only in this directory's
  # scope, so a parent project embedding woke via add_subdirectory() would see
  # its own *.mm sources reclassified from CXX to OBJCXX and fail to generate
  # ("Missing variable is: CMAKE_OBJCXX_COMPILE_OBJECT"). CMake lists `mm` in
  # CMAKE_CXX_SOURCE_FILE_EXTENSIONS, so the file compiles under the CXX rules
  # and clang picks Objective-C++ from the file extension.
  list(APPEND _woke_platform_sources
    src/backend_sleep_macos.cpp
    src/backend_nap_macos.mm)
elseif(UNIX)
  list(APPEND _woke_platform_sources
    src/backend_sleep_linux.cpp
    src/dbus_message.cpp
    src/backend_nap_linux.cpp)
else()
  message(FATAL_ERROR "woke: unsupported target platform")
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(woke
  src/woke.cpp
  ${_woke_platform_sources})
add_library(woke::woke ALIAS woke)

target_include_directories(woke
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_features(woke PUBLIC cxx_std_17)
set_target_properties(woke PROPERTIES
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)

if(APPLE)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
  find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
  target_link_libraries(woke PRIVATE
    ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY} ${FOUNDATION_LIBRARY})
  # The macOS nap backend manages the NSProcessInfo activity token with manual
  # retain/release, so it must be compiled without ARC.
  set_source_files_properties(src/backend_nap_macos.mm PROPERTIES
    COMPILE_OPTIONS "-fno-objc-arc")
endif()

if(MSVC)
  target_compile_options(woke PRIVATE /W4)
else()
  target_compile_options(woke PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS woke
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---------------------------------------------------------------------------
# Examples & tests
# ---------------------------------------------------------------------------
if(WOKE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(WOKE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
