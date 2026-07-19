#include "dbus_message.hpp"

namespace woke::detail {

namespace {

// Header field type codes (D-Bus spec, "Header Fields").
constexpr uint8_t kFieldPath = 1;
constexpr uint8_t kFieldInterface = 2;
constexpr uint8_t kFieldMember = 3;
constexpr uint8_t kFieldErrorName = 4;
constexpr uint8_t kFieldReplySerial = 5;
constexpr uint8_t kFieldDestination = 6;
constexpr uint8_t kFieldSignature = 8;
constexpr uint8_t kFieldUnixFds = 9;

size_t align_up(size_t n, size_t alignment) {
  return (n + (alignment - 1)) & ~(alignment - 1);
}

void pad_to(std::vector<uint8_t>& v, size_t alignment) {
  while (v.size() % alignment != 0) v.push_back(0);
}

void put_u32_le(std::vector<uint8_t>& v, uint32_t x) {
  v.push_back(static_cast<uint8_t>(x & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 8) & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 16) & 0xff));
  v.push_back(static_cast<uint8_t>((x >> 24) & 0xff));
}

// A D-Bus STRING / OBJECT_PATH: 4-byte aligned length, bytes, trailing NUL.
void put_string(std::vector<uint8_t>& v, const std::string& s) {
  pad_to(v, 4);
  put_u32_le(v, static_cast<uint32_t>(s.size()));
  v.insert(v.end(), s.begin(), s.end());
  v.push_back(0);
}

// A D-Bus SIGNATURE: single-byte length, bytes, trailing NUL (no alignment).
void put_signature(std::vector<uint8_t>& v, const std::string& s) {
  v.push_back(static_cast<uint8_t>(s.size()));
  v.insert(v.end(), s.begin(), s.end());
  v.push_back(0);
}

// Append a header field whose value is a STRING/OBJECT_PATH.
void put_field_string(std::vector<uint8_t>& fields, uint8_t code, char type,
                      const std::string& value) {
  pad_to(fields, 8);  // each header-field struct is 8-aligned
  fields.push_back(code);
  put_signature(fields, std::string(1, type));  // variant signature
  put_string(fields, value);
}

// Append a header field whose value is itself a SIGNATURE (used for field 8).
void put_field_signature(std::vector<uint8_t>& fields, uint8_t code,
                         const std::string& sig) {
  pad_to(fields, 8);
  fields.push_back(code);
  put_signature(fields, "g");  // variant signature
  put_signature(fields, sig);  // value
}

std::vector<uint8_t> build_message(uint8_t type, uint32_t serial,
                                   const std::string& destination,
                                   const std::string& path,
                                   const std::string& interface,
                                   const std::string& member,
                                   const std::string& signature,
                                   const std::vector<uint8_t>& body) {
  std::vector<uint8_t> fields;
  put_field_string(fields, kFieldPath, 'o', path);
  if (!destination.empty())
    put_field_string(fields, kFieldDestination, 's', destination);
  if (!interface.empty())
    put_field_string(fields, kFieldInterface, 's', interface);
  put_field_string(fields, kFieldMember, 's', member);
  if (!signature.empty())
    put_field_signature(fields, kFieldSignature, signature);

  std::vector<uint8_t> msg;
  msg.push_back('l');   // little-endian
  msg.push_back(type);  // message type
  msg.push_back(0);     // flags
  msg.push_back(1);     // protocol version
  put_u32_le(msg, static_cast<uint32_t>(body.size()));    // body length
  put_u32_le(msg, serial);                                // serial
  put_u32_le(msg, static_cast<uint32_t>(fields.size()));  // header array length
  msg.insert(msg.end(), fields.begin(), fields.end());
  pad_to(msg, 8);  // body starts on an 8-byte boundary
  msg.insert(msg.end(), body.begin(), body.end());
  return msg;
}

bool rd_u32(const uint8_t* d, size_t len, size_t off, bool le, uint32_t& out) {
  if (off + 4 > len) return false;
  if (le) {
    out = static_cast<uint32_t>(d[off]) |
          (static_cast<uint32_t>(d[off + 1]) << 8) |
          (static_cast<uint32_t>(d[off + 2]) << 16) |
          (static_cast<uint32_t>(d[off + 3]) << 24);
  } else {
    out = static_cast<uint32_t>(d[off + 3]) |
          (static_cast<uint32_t>(d[off + 2]) << 8) |
          (static_cast<uint32_t>(d[off + 1]) << 16) |
          (static_cast<uint32_t>(d[off]) << 24);
  }
  return true;
}

}  // namespace

std::vector<uint8_t> build_method_call(uint32_t serial,
                                       const std::string& destination,
                                       const std::string& path,
                                       const std::string& interface,
                                       const std::string& member,
                                       const std::string& signature,
                                       const std::vector<std::string>& args) {
  std::vector<uint8_t> body;
  for (const auto& arg : args) put_string(body, arg);
  return build_message(static_cast<uint8_t>(MsgType::MethodCall), serial,
                       destination, path, interface, member, signature, body);
}

bool parse_message(const uint8_t* d, size_t len, ParsedMessage& out,
                   size_t& consumed) {
  if (len < 16) return false;

  const uint8_t endian = d[0];
  if (endian != 'l' && endian != 'B') return false;
  const bool le = (endian == 'l');

  out = ParsedMessage{};
  out.little_endian = le;
  out.type = static_cast<MsgType>(d[1]);
  out.flags = d[2];

  uint32_t body_len = 0;
  uint32_t fields_len = 0;
  if (!rd_u32(d, len, 4, le, body_len)) return false;
  if (!rd_u32(d, len, 8, le, out.serial)) return false;
  if (!rd_u32(d, len, 12, le, fields_len)) return false;

  const size_t fields_start = 16;
  const size_t fields_end = fields_start + fields_len;
  if (fields_end > len) return false;
  const size_t body_start = align_up(fields_end, 8);
  if (body_start + body_len > len) return false;  // incomplete message

  // Walk the header-field array.
  size_t pos = fields_start;
  while (true) {
    pos = align_up(pos, 8);
    if (pos >= fields_end) break;

    const uint8_t code = d[pos++];
    if (pos >= fields_end) return false;
    const uint8_t sig_len = d[pos++];
    if (pos + static_cast<size_t>(sig_len) + 1 > fields_end) return false;
    const char vtype = (sig_len > 0) ? static_cast<char>(d[pos]) : '\0';
    pos += static_cast<size_t>(sig_len) + 1;  // signature bytes + NUL

    switch (vtype) {
      case 'u': {  // UINT32
        pos = align_up(pos, 4);
        uint32_t value = 0;
        if (!rd_u32(d, fields_end, pos, le, value)) return false;
        pos += 4;
        if (code == kFieldReplySerial) {
          out.reply_serial = value;
          out.has_reply_serial = true;
        } else if (code == kFieldUnixFds) {
          out.unix_fds = value;
          out.has_unix_fds = true;
        }
        break;
      }
      case 's':    // STRING
      case 'o': {  // OBJECT_PATH
        pos = align_up(pos, 4);
        uint32_t slen = 0;
        if (!rd_u32(d, fields_end, pos, le, slen)) return false;
        pos += 4;
        if (pos + static_cast<size_t>(slen) + 1 > fields_end) return false;
        std::string value(reinterpret_cast<const char*>(d + pos), slen);
        pos += static_cast<size_t>(slen) + 1;
        if (code == kFieldErrorName) out.error_name = std::move(value);
        break;
      }
      case 'g': {  // SIGNATURE
        if (pos >= fields_end) return false;
        const uint8_t glen = d[pos++];
        if (pos + static_cast<size_t>(glen) + 1 > fields_end) return false;
        std::string value(reinterpret_cast<const char*>(d + pos), glen);
        pos += static_cast<size_t>(glen) + 1;
        if (code == kFieldSignature) out.signature = std::move(value);
        break;
      }
      default:
        // A header field of a type we don't model.
        // We can't compute its length safely, so give up rather than misparse.
        return false;
    }
  }

  out.body.assign(d + body_start, d + body_start + body_len);
  consumed = body_start + body_len;
  return true;
}

bool message_length(const uint8_t* data, size_t len, size_t& total) {
  if (len < 16) return false;
  if (data[0] != 'l' && data[0] != 'B') return false;
  const bool le = (data[0] == 'l');
  uint32_t body_len = 0;
  uint32_t fields_len = 0;
  if (!rd_u32(data, len, 4, le, body_len)) return false;
  if (!rd_u32(data, len, 12, le, fields_len)) return false;
  total = align_up(16 + static_cast<size_t>(fields_len), 8) + body_len;
  return true;
}

bool read_uint32(const std::vector<uint8_t>& body, size_t offset,
                 bool little_endian, uint32_t& out) {
  return rd_u32(body.data(), body.size(), offset, little_endian, out);
}

bool read_string(const std::vector<uint8_t>& body, size_t offset,
                 bool little_endian, std::string& out, size_t* next) {
  offset = align_up(offset, 4);
  uint32_t len = 0;
  if (!rd_u32(body.data(), body.size(), offset, little_endian, len)) return false;
  const size_t start = offset + 4;
  if (start + static_cast<size_t>(len) + 1 > body.size()) return false;
  out.assign(reinterpret_cast<const char*>(body.data() + start), len);
  if (next) *next = start + static_cast<size_t>(len) + 1;
  return true;
}

}  // namespace woke::detail
