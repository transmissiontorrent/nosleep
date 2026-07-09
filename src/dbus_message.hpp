// Minimal D-Bus message (de)serialization.
//
// This is *not* a general D-Bus implementation. It supports exactly what the
// Linux backend needs:
//   * building METHOD_CALL messages whose body is zero or more strings, or a
//     single uint32;
//   * parsing a received message far enough to identify the reply it answers,
//     its type (return vs error) and its body.
//
// All messages we emit are little-endian; parsing honours the endianness flag
// in the received header. See the D-Bus specification, section "Message
// Protocol", for the wire format.

#ifndef NOSLEEP_SRC_DBUS_MESSAGE_HPP
#define NOSLEEP_SRC_DBUS_MESSAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nosleep::detail {

enum class MsgType : uint8_t {
  Invalid = 0,
  MethodCall = 1,
  MethodReturn = 2,
  Error = 3,
  Signal = 4,
};

// Build a METHOD_CALL whose body is the given string arguments. `signature`
// must be one 's' per argument (or empty for no arguments).
std::vector<uint8_t> build_method_call(uint32_t serial,
                                       const std::string& destination,
                                       const std::string& path,
                                       const std::string& interface,
                                       const std::string& member,
                                       const std::string& signature,
                                       const std::vector<std::string>& args);

struct ParsedMessage {
  MsgType type = MsgType::Invalid;
  uint8_t flags = 0;
  bool little_endian = true;
  uint32_t serial = 0;
  uint32_t reply_serial = 0;
  bool has_reply_serial = false;
  uint32_t unix_fds = 0;      // header field 9: number of attached file descriptors
  bool has_unix_fds = false;
  std::string signature;   // body signature (header field 8)
  std::string error_name;  // header field 4, set for Error messages
  std::vector<uint8_t> body;
};

// Parse a single message from the front of [data, data+len). On success returns
// true, fills `out`, and sets `consumed` to the number of bytes the message
// occupied. Returns false if the buffer does not yet contain a complete message
// or the data is malformed.
bool parse_message(const uint8_t* data, size_t len, ParsedMessage& out,
                   size_t& consumed);

// Compute the full on-wire length of the message whose header starts at `data`.
// Returns false if fewer than the 16 fixed header bytes are present; otherwise
// sets `total` to header + fields + padding + body. Lets a streaming reader know
// how many bytes to buffer before calling parse_message().
bool message_length(const uint8_t* data, size_t len, size_t& total);

// Read a uint32 from `body` at `offset` (with the given endianness). Returns
// false if the buffer is too short.
bool read_uint32(const std::vector<uint8_t>& body, size_t offset,
                 bool little_endian, uint32_t& out);

// Read a length-prefixed string from `body` at (4-aligned) `offset`. If `next`
// is non-null it receives the offset just past the string's trailing NUL.
bool read_string(const std::vector<uint8_t>& body, size_t offset,
                 bool little_endian, std::string& out, size_t* next = nullptr);

}  // namespace nosleep::detail

#endif  // NOSLEEP_SRC_DBUS_MESSAGE_HPP
