// Linux backend: systemd-logind "sleep" inhibitor, spoken over D-Bus directly
// (no libdbus / libsystemd dependency).
//
//   org.freedesktop.login1.Manager.Inhibit(what, who, why, mode) -> fd
//
// We request what="sleep", mode="block", which prevents the system from
// suspending or hibernating while deliberately leaving idle / screensaver
// behaviour alone -- exactly what a long-running network transfer wants. The
// call lives on the *system* bus (so it also works without a desktop session),
// and is served by systemd-logind or the compatible elogind on non-systemd
// distributions.
//
// logind returns a file descriptor; the inhibitor is held for as long as that
// descriptor stays open, independently of the D-Bus connection. So we grab the
// fd, drop the bus connection, and release simply by close()-ing the fd. The
// descriptor is delivered out-of-band via SCM_RIGHTS, which is why the auth
// handshake negotiates UNIX-FD passing and replies are read with recvmsg().
//
// If no system bus / logind is reachable, inhibit() fails gracefully.

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "backend.hpp"
#include "dbus_message.hpp"

namespace woke::detail {

namespace {

constexpr int kIoTimeoutSeconds = 5;

// ---- low-level socket helpers -------------------------------------------

void set_io_timeout(int fd) {
  timeval tv{};
  tv.tv_sec = kIoTimeoutSeconds;
  tv.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

bool write_all(int fd, const void* buf, size_t len) {
  const auto* p = static_cast<const uint8_t*>(buf);
  size_t sent = 0;
  while (sent < len) {
    const ssize_t n = ::write(fd, p + sent, len - sent);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (n == 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

bool write_all(int fd, const std::vector<uint8_t>& v) {
  return write_all(fd, v.data(), v.size());
}

// Read a single '\r\n'-terminated line (used only during SASL auth, which never
// carries file descriptors, so a plain read() is fine here).
bool read_line(int fd, std::string& out) {
  out.clear();
  char c = 0;
  for (;;) {
    const ssize_t n = ::read(fd, &c, 1);
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (n == 0) return false;
    if (c == '\n') {
      if (!out.empty() && out.back() == '\r') out.pop_back();
      return true;
    }
    out.push_back(c);
    if (out.size() > 4096) return false;  // runaway guard
  }
}

// ---- bus address handling ------------------------------------------------

// Parse a D-Bus address value, returning the first usable unix socket.
bool parse_bus_address(const std::string& address, std::string& path,
                       bool& is_abstract) {
  size_t start = 0;
  while (start <= address.size()) {
    const size_t semi = address.find(';', start);
    const std::string entry = address.substr(
        start, semi == std::string::npos ? std::string::npos : semi - start);
    start = (semi == std::string::npos) ? address.size() + 1 : semi + 1;

    if (entry.rfind("unix:", 0) != 0) continue;
    const std::string opts = entry.substr(5);

    size_t ostart = 0;
    while (ostart <= opts.size()) {
      const size_t comma = opts.find(',', ostart);
      const std::string kv = opts.substr(
          ostart, comma == std::string::npos ? std::string::npos : comma - ostart);
      ostart = (comma == std::string::npos) ? opts.size() + 1 : comma + 1;

      if (kv.rfind("path=", 0) == 0) {
        path = kv.substr(5);
        is_abstract = false;
        return true;
      }
      if (kv.rfind("abstract=", 0) == 0) {
        path = kv.substr(9);
        is_abstract = true;
        return true;
      }
    }
  }
  return false;
}

void resolve_system_bus_address(std::string& path, bool& is_abstract) {
  if (const char* env = std::getenv("DBUS_SYSTEM_BUS_ADDRESS")) {
    if (parse_bus_address(env, path, is_abstract)) return;
  }
  path = "/var/run/dbus/system_bus_socket";
  is_abstract = false;
}

int connect_unix(const std::string& path, bool is_abstract) {
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  const size_t max_path = sizeof(addr.sun_path);

  socklen_t addr_len = 0;
  if (is_abstract) {
    // Abstract sockets: leading NUL, then the name (not NUL-terminated).
    if (path.size() + 1 > max_path) return -1;
    addr.sun_path[0] = '\0';
    std::memcpy(addr.sun_path + 1, path.data(), path.size());
    addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 +
                                      path.size());
  } else {
    if (path.size() + 1 > max_path) return -1;
    std::memcpy(addr.sun_path, path.data(), path.size());
    addr_len = static_cast<socklen_t>(sizeof(addr));
  }

  const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  set_io_timeout(fd);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), addr_len) != 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

// ---- SASL authentication (EXTERNAL / uid, with UNIX-FD negotiation) -------

std::string hex_encode(const std::string& s) {
  static const char* digits = "0123456789abcdef";
  std::string out;
  out.reserve(s.size() * 2);
  for (unsigned char c : s) {
    out.push_back(digits[c >> 4]);
    out.push_back(digits[c & 0x0f]);
  }
  return out;
}

bool authenticate(int fd) {
  // D-Bus requires a leading NUL byte before the SASL exchange.
  const uint8_t nul = 0;
  if (!write_all(fd, &nul, 1)) return false;

  const std::string uid = std::to_string(static_cast<unsigned>(::getuid()));
  const std::string auth = "AUTH EXTERNAL " + hex_encode(uid) + "\r\n";
  if (!write_all(fd, auth.data(), auth.size())) return false;

  std::string line;
  if (!read_line(fd, line) || line.rfind("OK", 0) != 0) return false;

  // Negotiate the ability to receive file descriptors; logind hands us one.
  const std::string negotiate = "NEGOTIATE_UNIX_FD\r\n";
  if (!write_all(fd, negotiate.data(), negotiate.size())) return false;
  if (!read_line(fd, line) || line.rfind("AGREE_UNIX_FD", 0) != 0) return false;

  const std::string begin = "BEGIN\r\n";
  return write_all(fd, begin.data(), begin.size());
}

// ---- message exchange with file-descriptor passing -----------------------

// A bus connection plus the stream / fd receive buffers. Owns the socket and
// any received-but-not-yet-consumed descriptors.
struct Conn {
  int fd = -1;
  std::vector<uint8_t> buf;  // buffered stream bytes not yet parsed
  std::vector<int> fds;      // received descriptors, in arrival order

  Conn() = default;
  Conn(const Conn&) = delete;
  Conn& operator=(const Conn&) = delete;
  ~Conn() {
    if (fd >= 0) ::close(fd);
    for (int f : fds) ::close(f);
  }
};

// One recvmsg() into the connection buffers, capturing any SCM_RIGHTS fds.
bool conn_fill(Conn& c) {
  uint8_t bytes[4096];
  alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(int) * 16)];

  iovec iov{};
  iov.iov_base = bytes;
  iov.iov_len = sizeof(bytes);

  msghdr msg{};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = sizeof(control);

  ssize_t n;
  do {
    n = ::recvmsg(c.fd, &msg, 0);
  } while (n < 0 && errno == EINTR);
  if (n <= 0) return false;
  if (msg.msg_flags & MSG_CTRUNC) return false;  // control buffer overflowed

  for (cmsghdr* cm = CMSG_FIRSTHDR(&msg); cm != nullptr;
       cm = CMSG_NXTHDR(&msg, cm)) {
    if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
      const size_t count = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
      for (size_t i = 0; i < count; ++i) {
        int received = -1;
        std::memcpy(&received, CMSG_DATA(cm) + i * sizeof(int), sizeof(int));
        c.fds.push_back(received);
      }
    }
  }

  c.buf.insert(c.buf.end(), bytes, bytes + static_cast<size_t>(n));
  return true;
}

// Read exactly one framed message off the connection. If the message carries a
// file descriptor, it is returned via `out_fd` (any extra fds are closed).
bool conn_read_message(Conn& c, ParsedMessage& out, int& out_fd) {
  out_fd = -1;
  for (int guard = 0; guard < 4096; ++guard) {
    size_t total = 0;
    if (message_length(c.buf.data(), c.buf.size(), total) &&
        c.buf.size() >= total) {
      size_t consumed = 0;
      if (!parse_message(c.buf.data(), total, out, consumed)) return false;

      if (out.has_unix_fds && out.unix_fds > 0) {
        if (c.fds.size() < out.unix_fds) return false;  // fd not here yet
        std::vector<int> mfds(c.fds.begin(), c.fds.begin() + out.unix_fds);
        c.fds.erase(c.fds.begin(), c.fds.begin() + out.unix_fds);
        // The body's uint32 is an index into this message's fd array.
        uint32_t index = 0;
        if (!read_uint32(out.body, 0, out.little_endian, index) ||
            index >= mfds.size()) {
          index = 0;
        }
        out_fd = mfds[index];
        for (size_t i = 0; i < mfds.size(); ++i) {
          if (i != index) ::close(mfds[i]);
        }
      }

      c.buf.erase(c.buf.begin(), c.buf.begin() + total);
      return true;
    }
    if (!conn_fill(c)) return false;
  }
  return false;
}

// Read messages until one answering `serial` arrives (ignoring unrelated
// signals such as NameAcquired).
bool conn_wait_for_reply(Conn& c, uint32_t serial, ParsedMessage& out,
                         int& out_fd) {
  for (int i = 0; i < 64; ++i) {
    int fd = -1;
    if (!conn_read_message(c, out, fd)) return false;
    if (out.has_reply_serial && out.reply_serial == serial) {
      out_fd = fd;
      return true;
    }
    if (fd >= 0) ::close(fd);  // unrelated message carrying an fd (unexpected)
  }
  return false;
}

// ---- backend -------------------------------------------------------------

class LinuxBackend final : public Backend {
public:
  ~LinuxBackend() override { uninhibit(); }

  bool inhibit(const std::string& who, const std::string& reason) override {
    if (active_) return true;

    std::string path;
    bool is_abstract = false;
    resolve_system_bus_address(path, is_abstract);

    Conn conn;
    conn.fd = connect_unix(path, is_abstract);
    if (conn.fd < 0) return false;
    if (!authenticate(conn.fd)) return false;

    // The bus requires Hello as the first method call.
    const auto hello =
        build_method_call(1, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                           "org.freedesktop.DBus", "Hello", "", {});
    ParsedMessage reply;
    int reply_fd = -1;
    if (!write_all(conn.fd, hello) ||
        !conn_wait_for_reply(conn, 1, reply, reply_fd) ||
        reply.type != MsgType::MethodReturn) {
      if (reply_fd >= 0) ::close(reply_fd);
      return false;
    }

    // login1.Manager.Inhibit("sleep", who, why, "block") -> file descriptor.
    const std::string who_field = who.empty() ? "woke" : who;
    const auto call = build_method_call(
        2, "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "Inhibit", "ssss",
        {"sleep", who_field, reason, "block"});

    ParsedMessage result;
    int lock_fd = -1;
    if (!write_all(conn.fd, call) ||
        !conn_wait_for_reply(conn, 2, result, lock_fd)) {
      if (lock_fd >= 0) ::close(lock_fd);
      return false;
    }
    if (result.type != MsgType::MethodReturn || lock_fd < 0) {
      if (lock_fd >= 0) ::close(lock_fd);
      return false;  // e.g. an error reply, or no descriptor delivered
    }

    // The inhibitor persists as long as lock_fd is open; the bus connection
    // (closed by ~Conn on return) is no longer needed.
    inhibit_fd_ = lock_fd;
    active_ = true;
    return true;
  }

  void uninhibit() noexcept override {
    if (inhibit_fd_ >= 0) {
      ::close(inhibit_fd_);  // releases the logind inhibitor lock
      inhibit_fd_ = -1;
    }
    active_ = false;
  }

  bool active() const noexcept override { return active_; }

private:
  int inhibit_fd_ = -1;
  bool active_ = false;
};

}  // namespace

std::unique_ptr<Backend> make_backend() {
  return std::make_unique<LinuxBackend>();
}

const char* backend_name() { return "linux-logind"; }

}  // namespace woke::detail
