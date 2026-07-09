# The test executables use a tiny built-in harness (no external framework), and
# each test *case* is registered with CTest separately so failures are reported
# individually.

# ---- public API tests ----------------------------------------------------
add_executable(nosleep_tests test_nosleep.cpp)
target_link_libraries(nosleep_tests PRIVATE nosleep)
target_compile_features(nosleep_tests PRIVATE cxx_std_17)

set(_api_cases
  backend_name
  construct_destruct
  uninhibit_without_inhibit
  inhibit_lifecycle
  reinhibit
  move_semantics)
foreach(case IN LISTS _api_cases)
  add_test(NAME api.${case} COMMAND nosleep_tests ${case})
endforeach()

# ---- Linux D-Bus marshalling unit tests ----------------------------------
# These exercise the wire-format code directly (it can't be reached through the
# public API without a live bus service), so they run on every Linux build.
if(UNIX AND NOT APPLE)
  add_executable(dbus_message_tests
    test_dbus_message.cpp
    ${CMAKE_SOURCE_DIR}/src/dbus_message.cpp)
  target_include_directories(dbus_message_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_compile_features(dbus_message_tests PRIVATE cxx_std_17)

  set(_dbus_cases
    build_hello
    roundtrip_inhibit
    roundtrip_login1
    read_uint32
    parse_incomplete
    body_alignment)
  foreach(case IN LISTS _dbus_cases)
    add_test(NAME dbus.${case} COMMAND dbus_message_tests ${case})
  endforeach()
endif()
