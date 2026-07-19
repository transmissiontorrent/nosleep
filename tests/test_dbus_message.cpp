// Unit tests for the D-Bus wire-format helpers (Linux only). These validate the
// marshalling/parsing code that the live bus path depends on but which cannot
// be reached through the public API without a running inhibitor service.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "dbus_message.hpp"
#include "test_util.hpp"

namespace {

using namespace woke::detail;

void test_build_hello() {
  const auto msg =
      build_method_call(1, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                        "org.freedesktop.DBus", "Hello", "", {});
  // Fixed header sanity.
  CHECK(msg.size() >= 16);
  CHECK(msg[0] == 'l');  // little-endian
  CHECK(msg[1] == 1);    // METHOD_CALL
  CHECK(msg[3] == 1);    // protocol version
  // No body, so the whole message is the padded header: a multiple of 8.
  CHECK(msg.size() % 8 == 0);

  ParsedMessage parsed;
  size_t consumed = 0;
  CHECK(parse_message(msg.data(), msg.size(), parsed, consumed));
  CHECK(consumed == msg.size());
  CHECK(parsed.type == MsgType::MethodCall);
  CHECK(parsed.serial == 1);
  CHECK(parsed.little_endian);
  CHECK(parsed.signature.empty());
  CHECK(parsed.body.empty());
}

void test_roundtrip_inhibit() {
  const std::string app = "example-app";
  const std::string reason = "playing a movie";
  const auto msg = build_method_call(
      7, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", "Inhibit", "ss", {app, reason});

  ParsedMessage parsed;
  size_t consumed = 0;
  CHECK(parse_message(msg.data(), msg.size(), parsed, consumed));
  CHECK(consumed == msg.size());
  CHECK(parsed.type == MsgType::MethodCall);
  CHECK(parsed.serial == 7);
  CHECK(parsed.signature == "ss");

  std::string got_app;
  std::string got_reason;
  size_t next = 0;
  CHECK(read_string(parsed.body, 0, parsed.little_endian, got_app, &next));
  CHECK(read_string(parsed.body, next, parsed.little_endian, got_reason));
  CHECK(got_app == app);
  CHECK(got_reason == reason);
}

void test_roundtrip_login1() {
  // The systemd-logind call the Linux backend actually makes.
  const std::vector<std::string> args = {"sleep", "example-app",
                                         "network transfer in progress", "block"};
  const auto msg = build_method_call(
      2, "org.freedesktop.login1", "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager", "Inhibit", "ssss", args);

  ParsedMessage parsed;
  size_t consumed = 0;
  CHECK(parse_message(msg.data(), msg.size(), parsed, consumed));
  CHECK(consumed == msg.size());
  CHECK(parsed.signature == "ssss");

  size_t offset = 0;
  for (const std::string& expected : args) {
    std::string got;
    CHECK(read_string(parsed.body, offset, parsed.little_endian, got, &offset));
    CHECK(got == expected);
  }
}

void test_read_uint32() {
  // The uint32 decode the reply path relies on (fd index / reply serial).
  uint32_t value = 0;
  const std::vector<uint8_t> little = {0xEF, 0xBE, 0xAD, 0xDE};
  CHECK(read_uint32(little, 0, /*little_endian=*/true, value));
  CHECK(value == 0xDEADBEEFu);

  const std::vector<uint8_t> big = {0xDE, 0xAD, 0xBE, 0xEF};
  CHECK(read_uint32(big, 0, /*little_endian=*/false, value));
  CHECK(value == 0xDEADBEEFu);

  // A short buffer is rejected rather than read out of bounds.
  const std::vector<uint8_t> too_short = {0x01, 0x02, 0x03};
  CHECK(!read_uint32(too_short, 0, /*little_endian=*/true, value));
}

void test_parse_incomplete() {
  const auto msg = build_method_call(
      3, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", "Inhibit", "ss", {"a", "bb"});

  // A truncated buffer must be rejected, not misparsed.
  ParsedMessage parsed;
  size_t consumed = 0;
  CHECK(!parse_message(msg.data(), msg.size() - 1, parsed, consumed));
  CHECK(!parse_message(msg.data(), 8, parsed, consumed));
}

void test_body_alignment() {
  const auto msg = build_method_call(
      5, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
      "org.freedesktop.ScreenSaver", "Inhibit", "ss", {"app", "why"});

  ParsedMessage parsed;
  size_t consumed = 0;
  CHECK(parse_message(msg.data(), msg.size(), parsed, consumed));
  CHECK(consumed == msg.size());
  // The body must begin on an 8-byte boundary within the message.
  const size_t body_start = consumed - parsed.body.size();
  CHECK(body_start % 8 == 0);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("usage: %s <case>\n", argv[0]);
    return 2;
  }
  const std::string test = argv[1];

  if (test == "build_hello") {
    test_build_hello();
  } else if (test == "roundtrip_inhibit") {
    test_roundtrip_inhibit();
  } else if (test == "roundtrip_login1") {
    test_roundtrip_login1();
  } else if (test == "read_uint32") {
    test_read_uint32();
  } else if (test == "parse_incomplete") {
    test_parse_incomplete();
  } else if (test == "body_alignment") {
    test_body_alignment();
  } else {
    std::printf("unknown test case: %s\n", test.c_str());
    return 2;
  }

  return test_report(test.c_str());
}
