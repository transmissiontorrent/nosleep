cmake_minimum_required(VERSION 3.16)

project(nosleep
  VERSION 0.1.0
  DESCRIPTION "Cross-platform library to inhibit desktop sleep / hibernation"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(NOSLEEP_BUILD_TESTS    "Build the nosleep test suite"  ON)
option(NOSLEEP_BUILD_EXAMPLES "Build the nosleep example app" ON)

# ---------------------------------------------------------------------------
# Select the platform backend
# ---------------------------------------------------------------------------
set(_nosleep_platform_sources)
if(WIN32)
  list(APPEND _nosleep_platform_sources src/backend_windows.cpp)
elseif(APPLE)
  list(APPEND _nosleep_platform_sources src/backend_macos.cpp)
elseif(UNIX)
  list(APPEND _nosleep_platform_sources src/backend_linux.cpp src/dbus_message.cpp)
else()
  message(FATAL_ERROR "nosleep: unsupported target platform")
endif()

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(nosleep
  src/nosleep.cpp
  ${_nosleep_platform_sources})
add_library(nosleep::nosleep ALIAS nosleep)

target_include_directories(nosleep
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_features(nosleep PUBLIC cxx_std_17)
set_target_properties(nosleep PROPERTIES
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON)

if(APPLE)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
  target_link_libraries(nosleep PRIVATE ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
endif()

if(MSVC)
  target_compile_options(nosleep PRIVATE /W4)
else()
  target_compile_options(nosleep PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS nosleep
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---------------------------------------------------------------------------
# Examples & tests
# ---------------------------------------------------------------------------
if(NOSLEEP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(NOSLEEP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
