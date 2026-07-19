cmake_minimum_required(VERSION 3.16)

project(woke
  VERSION 0.1.0
  DESCRIPTION "Cross-platform library to inhibit desktop sleep / hibernation"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(WOKE_BUILD_TESTS    "Build the woke test suite"  ON)
option(WOKE_BUILD_EXAMPLES "Build the woke example app" ON)

# ---------------------------------------------------------------------------
# Select the platform backend
# ---------------------------------------------------------------------------
set(_woke_platform_sources)
if(WIN32)
  list(APPEND _woke_platform_sources src/backend_windows.cpp)
elseif(APPLE)
  list(APPEND _woke_platform_sources src/backend_macos.cpp)
elseif(UNIX)
  list(APPEND _woke_platform_sources src/backend_linux.cpp src/dbus_message.cpp)
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
  target_link_libraries(woke PRIVATE ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
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
