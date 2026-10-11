#include "net.hpp"
#include "shape.hpp"
#include <bit>
#include <concepts>
#include <cstddef>
#include <optional>
#include <print>

namespace pboard::net {

bool ClientState::is_descendant(ObjectId origin, ObjectId candidate) {
  auto it = objs.find(candidate);
  if (it == objs.end())
    return false;

  ObjectId parent = it->second.origin;

  while (parent != OBJ_NONE) {
    if (parent == origin) {
      return true;
    }

    auto parent_it = objs.find(parent);
    if (parent_it == objs.end()) {
      return false;
    }

    parent = parent_it->second.origin;
  }

  return false;
}

void ClientState::insert(ObjectId id, Object obj, ObjectId origin) {
  ObjectId next = OBJ_NONE;

  if (origin == OBJ_NONE) {
    ObjectId curr = head;
    while (curr != OBJ_NONE) {
      if (objs.at(curr).origin == OBJ_NONE && id < curr) {
        next = curr;
        break;
      }
      curr = objs.at(curr).next;
    }
  } else {
    ObjectId curr = objs.at(origin).next;
    while (curr != OBJ_NONE) {
      if (!is_descendant(origin, curr)) {
        next = curr;
        break;
      }

      if (objs.at(curr).origin == origin && id < curr) {
        next = curr;
        break;
      }

      curr = objs.at(curr).next;
    }
  }

  ObjectId prev = (next == OBJ_NONE) ? last : objs.at(next).prev;

  obj.prev = prev;
  obj.next = next;
  auto [it, ins] = objs.emplace(id, obj);

  if (!ins) {
    return;
  }

  if (prev == OBJ_NONE) {
    head = id;
  } else {
    objs.at(prev).next = id;
  }

  if (next == OBJ_NONE) {
    last = id;
  } else {
    objs.at(next).prev = id;
  }

  flush_buffer();
}

void ClientState::push_back(shape::Shape shape, ObjectId origin) {
  ObjectId id{self, gen++};
  Object obj{shape, origin, OBJ_NONE, OBJ_NONE};

  Message msg{Message::Create{id, shape, origin}};
  std::vector<std::uint8_t> buf{};
  msg.serialize(buf);

  if (is_host) {
    asio::error_code ec;
    for (const auto &ep : host_eps) {
      sock.send_to(asio::buffer(buf), ep, 0, ec);
      if (ec) {
        std::println("LOG: Failed to send to {}:{}", ep.address().to_string(),
                     ep.port());
      }
    }
  } else {
    asio::error_code ec;
    sock.send(asio::buffer(buf), 0, ec);
    if (ec) {
      std::println("LOG: Failed to send to host");
    }
  }

  insert(id, obj, origin);
}

void ClientState::erase(ObjectId id) {
  if (auto it = objs.find(id); it != objs.end()) {
    ObjectId by{self, gen++};
    it->second.deleted_by = by;
    Message msg{Message::Delete{by, id}};
    std::vector<std::uint8_t> buf{};
    msg.serialize(buf);
    if (is_host) {
      asio::error_code ec;
      for (const auto &ep : host_eps) {
        sock.send_to(asio::buffer(buf), ep, 0, ec);
        if (ec) {
          std::println("LOG: Failed to send to {}:{}", ep.address().to_string(),
                       ep.port());
        }
      }
    } else {
      asio::error_code ec;
      sock.send(asio::buffer(buf), 0, ec);
      if (ec) {
        std::println("LOG: Failed to send to host");
      }
    }

    flush_buffer();
  }
}

void ClientState::flush_buffer() {
  bool progress{false};
  do {
    for (auto it = buffer.begin(); it != buffer.end();) {
      if (objs.contains(it->depends)) {
        auto bm = *it;
        buffer.erase(it);
        progress = true;
        handle(bm.sender, bm.msg);
        break;
      } else {
        ++it;
      }
    }
  } while (progress);
}

void ClientState::intro() {
  if (is_host) {
    self = uuids::uuid_system_generator{}();
  } else {
    asking = uuids::uuid_system_generator{}();
    Message msg{Message::Hi{asking}};
    std::vector<std::uint8_t> buf;
    msg.serialize(buf);
    asio::error_code ec;
    sock.send(asio::buffer(buf), 0, ec);
    if (ec) {
      std::println("LOG: Failed to send hi, intro");
    }
  }
}

void ClientState::handle(asio::ip::udp::endpoint sender, Message msg) {
  struct {
    asio::ip::udp::endpoint sender;
    ClientState &self;

    void operator()(const Message::Create &data) {
      self.versions.try_emplace(data.id.owner);

      if (self.objs.contains(data.after) || data.after == OBJ_NONE) {
        auto &ver = self.versions[data.id.owner];
        if (data.id.gen < ver.prefix)
          return;
        if (data.id.gen >= ver.prefix + WINDOW_MAX) {
          std::println("Creation past the window?");
          return; // TODO: Disconnected? synchronise here
        }
        if (ver.wip[data.id.gen - ver.prefix])
          return;
        self.insert(data.id, Object{data.shape, data.after, OBJ_NONE, OBJ_NONE},
                    data.after);
        ver.wip[data.id.gen - ver.prefix] = true;
        ver.compact();
      } else {
        std::println("Buffering creation ({}, {})", data.id.owner, data.id.gen);
        self.buffer.push_back(BufferedMessage{sender, {data}, data.after});
      }
    }

    void operator()(const Message::Delete &data) {
      self.versions.try_emplace(data.by.owner);

      if (auto it = self.objs.find(data.target); it != self.objs.end()) {
        auto &ver = self.versions[data.by.owner];
        if (data.by.gen < ver.prefix)
          return;
        if (data.by.gen >= ver.prefix + WINDOW_MAX) {
          std::println("Deletion past the window?");
          return; // TODO: Disconnected? synchronise here
        }
        if (ver.wip[data.by.gen - ver.prefix])
          return;
        it->second.deleted_by = data.by;
        ver.wip[data.by.gen - ver.prefix] = true;
        ver.compact();
        self.flush_buffer();
      } else {
        std::println("Buffering deletion ({}, {})", data.target.owner,
                     data.target.gen);
        self.buffer.push_back(BufferedMessage{sender, {data}, data.target});
      }
    }

    void operator()(const Message::Hi &data) {
      if (self.is_host) {
        // TODO with versions:
        Message msg{Message::Ih{true, data.ask}};
        std::vector<std::uint8_t> buf;
        msg.serialize(buf);
        asio::error_code ec;
        self.sock.send_to(asio::buffer(buf), sender, 0, ec);
        if (ec) {
          std::println("LOG: Failed to respond to Hi");
        }
      }
    }

    void operator()(const Message::Ih &data) {
      bool asked_it = data.assign == self.asking;
      if (!self.is_host && asked_it && data.permit) {
        self.self = data.assign;
      }
      self.asking = {};
    }
  } vtor{sender, *this};

  std::visit(vtor, msg.data);
}

void ClientState::update() {
  std::array<std::uint8_t, 1024> buffer{};
  asio::ip::udp::endpoint ep;
  asio::error_code ec;

  std::size_t recv;

  if (is_host) {
    recv = sock.receive_from(asio::buffer(buffer), ep, 0, ec);
  } else {
    recv = sock.receive(asio::buffer(buffer), 0, ec);
    ep = sock.remote_endpoint();
  }

  if (!ec && recv > 0) {
    std::span<const std::uint8_t> packet{buffer.data(), recv};

    if (is_host) {
      auto it = std::find(host_eps.begin(), host_eps.end(), ep);
      if (it == host_eps.end()) {
        std::println("Discovered {}:{}", ep.address().to_string(), ep.port());
        host_eps.push_back(ep);
      }

      for (const auto &other : host_eps) {
        if (other == ep)
          continue;
        sock.send_to(asio::buffer(packet), other, 0, ec);
      }
    }

    auto msg = Message::deserialize(packet);
    if (msg) {
      std::println("{}", msg->to_string());
      handle(ep, *msg);
    } else {
      std::println("Failed to decode message");
    }
  } else if (ec == asio::error::would_block || ec == asio::error::try_again) {
    // OK
  } else {
    std::println("LOG: Socket receive failed");
  }

  if (self.is_nil() && asking.is_nil()) {
    intro();
  }
}

void VersionWindow::compact() {
  Generation slide = 0;
  for (auto b : wip) {
    if (b) {
      slide++;
    } else {
      break;
    }
  }

  if (slide == 0)
    return;

  prefix += slide;
  std::copy(wip.begin() + slide, wip.end(), wip.begin());
  std::fill(wip.end() - slide, wip.end(), false);
}

template <typename T>
  requires std::integral<T> || std::floating_point<T>
void serialize_num(T n, std::vector<std::uint8_t> &out) {
  auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(n);
  if constexpr (std::endian::native == std::endian::big) {
    std::ranges::reverse(bytes);
  }
  out.insert(out.end(), bytes.begin(), bytes.end());
}

void serialize_id(ClientId id, std::vector<std::uint8_t> &out) {
  auto bs = id.as_bytes();
  const auto *ptr = reinterpret_cast<const std::uint8_t *>(bs.data());
  out.insert(out.end(), ptr, ptr + bs.size());
}

void serialize_shape(shape::Shape shape, std::vector<std::uint8_t> &out) {
  struct {
    std::vector<std::uint8_t> &out;

    void operator()(const shape::Line &line) {
      out.push_back('L');
      out.push_back('N');
      serialize_num(line.x0, out);
      serialize_num(line.x1, out);
      serialize_num(line.y0, out);
      serialize_num(line.y1, out);
      serialize_num(line.thick, out);
      serialize_num(line.col.r, out);
      serialize_num(line.col.g, out);
      serialize_num(line.col.b, out);
    }

    void operator()(const shape::Rect &rect) {
      out.push_back('R');
      out.push_back('T');
      serialize_num(rect.x, out);
      serialize_num(rect.y, out);
      serialize_num(rect.w, out);
      serialize_num(rect.h, out);
      serialize_num(rect.col.r, out);
      serialize_num(rect.col.g, out);
      serialize_num(rect.col.b, out);
    }

    void operator()(const shape::Circ &circ) {
      out.push_back('C');
      out.push_back('C');
      serialize_num(circ.x, out);
      serialize_num(circ.y, out);
      serialize_num(circ.r, out);
      serialize_num(circ.col.r, out);
      serialize_num(circ.col.g, out);
      serialize_num(circ.col.b, out);
    }
  } serial{out};

  std::visit(serial, shape);
}

void Message::serialize(std::vector<std::uint8_t> &out) {
  out.push_back('P');
  out.push_back('B');
  std::size_t sz_idx = out.size();
  out.resize(out.size() + 4); // Packet size
  std::size_t start = out.size();

  struct {
    std::vector<std::uint8_t> &out;
    void operator()(const Create &data) {
      out.push_back('C');
      out.push_back('R');
      serialize_id(data.id.owner, out);
      serialize_num(data.id.gen, out);
      serialize_id(data.after.owner, out);
      serialize_num(data.after.gen, out);
      serialize_shape(data.shape, out);
    }

    void operator()(const Hi &data) {
      out.push_back('H');
      out.push_back('I');
      serialize_id(data.ask, out);
    }

    void operator()(const Ih &data) {
      out.push_back('I');
      out.push_back('H');
      out.push_back(data.permit ? 'T' : 'F');
      serialize_id(data.assign, out);
    }

    void operator()(const Delete &data) {
      out.push_back('D');
      out.push_back('L');
      serialize_id(data.by.owner, out);
      serialize_num(data.by.gen, out);
      serialize_id(data.target.owner, out);
      serialize_num(data.target.gen, out);
    }
  } serial{out};

  std::visit(serial, data);
  std::size_t len = out.size() - start;
  auto bytes = std::bit_cast<std::array<std::uint8_t, 4>>(
      static_cast<std::uint32_t>(len));
  if constexpr (std::endian::native == std::endian::big) {
    std::ranges::reverse(bytes);
  }
  std::copy(bytes.begin(), bytes.end(), out.begin() + sz_idx);
}

template <typename T>
  requires std::integral<T> || std::floating_point<T>
bool deserialize_num(T &val, auto &it, auto end) {
  constexpr std::size_t N = sizeof(T);
  if (std::distance(it, end) < static_cast<std::ptrdiff_t>(N)) {
    return false;
  }
  std::array<std::uint8_t, N> bytes;
  std::copy_n(it, N, bytes.begin());
  it += N;

  if constexpr (std::endian::native == std::endian::big) {
    std::ranges::reverse(bytes);
  }
  val = std::bit_cast<T>(bytes);
  return true;
}

bool deserialize_id(ClientId &val, auto &it, auto end) {
  if (std::distance(it, end) < 16) {
    return false;
  }

  std::array<std::uint8_t, 16> bytes;
  std::copy_n(it, 16, bytes.begin());
  it += 16;

  val = uuids::uuid{bytes};
  return true;
}

std::optional<shape::Shape> deserialize_shape(auto &it, auto end) {
  if (std::distance(it, end) < 2)
    return std::nullopt;
  char tag1 = *it++;
  char tag2 = *it++;

  if (tag1 == 'L' && tag2 == 'N') {
    shape::Line line;
    if (!deserialize_num(line.x0, it, end))
      return std::nullopt;
    if (!deserialize_num(line.x1, it, end))
      return std::nullopt;
    if (!deserialize_num(line.y0, it, end))
      return std::nullopt;
    if (!deserialize_num(line.y1, it, end))
      return std::nullopt;
    if (!deserialize_num(line.thick, it, end))
      return std::nullopt;
    if (!deserialize_num(line.col.r, it, end))
      return std::nullopt;
    if (!deserialize_num(line.col.g, it, end))
      return std::nullopt;
    if (!deserialize_num(line.col.b, it, end))
      return std::nullopt;
    return shape::Shape{line};
  } else if (tag1 == 'R' && tag2 == 'T') {
    shape::Rect rect;
    if (!deserialize_num(rect.x, it, end))
      return std::nullopt;
    if (!deserialize_num(rect.y, it, end))
      return std::nullopt;
    if (!deserialize_num(rect.w, it, end))
      return std::nullopt;
    if (!deserialize_num(rect.h, it, end))
      return std::nullopt;
    if (!deserialize_num(rect.col.r, it, end))
      return std::nullopt;
    if (!deserialize_num(rect.col.g, it, end))
      return std::nullopt;
    if (!deserialize_num(rect.col.b, it, end))
      return std::nullopt;
    return shape::Shape{rect};
  } else if (tag1 == 'C' && tag2 == 'C') {
    shape::Circ circ;
    if (!deserialize_num(circ.x, it, end))
      return std::nullopt;
    if (!deserialize_num(circ.y, it, end))
      return std::nullopt;
    if (!deserialize_num(circ.r, it, end))
      return std::nullopt;
    if (!deserialize_num(circ.col.r, it, end))
      return std::nullopt;
    if (!deserialize_num(circ.col.g, it, end))
      return std::nullopt;
    if (!deserialize_num(circ.col.b, it, end))
      return std::nullopt;
    return shape::Shape{circ};
  }

  return std::nullopt;
}

std::optional<Message::Var> deserialize_var(auto &it, auto end) {
  if (std::distance(it, end) < 2)
    return std::nullopt;
  char tag1 = *it++;
  char tag2 = *it++;

  if (tag1 == 'C' && tag2 == 'R') {
    Message::Create data;
    if (!deserialize_id(data.id.owner, it, end))
      return std::nullopt;
    if (!deserialize_num(data.id.gen, it, end))
      return std::nullopt;
    if (!deserialize_id(data.after.owner, it, end))
      return std::nullopt;
    if (!deserialize_num(data.after.gen, it, end))
      return std::nullopt;

    auto shape_opt = deserialize_shape(it, end);
    if (!shape_opt)
      return std::nullopt;
    data.shape = *shape_opt;

    return Message::Var{data};
  } else if (tag1 == 'H' && tag2 == 'I') {
    Message::Hi data;
    if (!deserialize_id(data.ask, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'I' && tag2 == 'H') {
    Message::Ih data;
    if (std::distance(it, end) < 1)
      return std::nullopt;
    switch (*it++) {
    case 'T':
      data.permit = true;
      break;
    case 'F':
      data.permit = false;
      break;
    default:
      return std::nullopt;
    }
    if (!deserialize_id(data.assign, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'D' && tag2 == 'L') {
    Message::Delete data;
    if (!deserialize_id(data.by.owner, it, end))
      return std::nullopt;
    if (!deserialize_num(data.by.gen, it, end))
      return std::nullopt;
    if (!deserialize_id(data.target.owner, it, end))
      return std::nullopt;
    if (!deserialize_num(data.target.gen, it, end))
      return std::nullopt;
    return Message::Var{data};
  }

  return std::nullopt;
}

std::optional<Message> Message::deserialize(std::span<const std::uint8_t> buf) {
  if (buf.size() < 6) {
    return std::nullopt;
  }

  if (buf[0] != 'P' || buf[1] != 'B') {
    return std::nullopt;
  }

  auto it = buf.begin() + 2;
  std::uint32_t len = 0;
  if (!deserialize_num(len, it, buf.end())) {
    return std::nullopt;
  }

  if (static_cast<std::size_t>(std::distance(it, buf.end())) < len) {
    return std::nullopt;
  }

  auto payload_end = it + len;
  auto var_opt = deserialize_var(it, payload_end);
  if (!var_opt) {
    return std::nullopt;
  }

  Message msg;
  msg.data = *var_opt;
  return msg;
}

std::string Message::to_string() const {
  struct {
    std::string operator()(const Create &data) {
      return std::format("Create ({}, {}) at ({}, {}), {}", data.id.owner,
                         data.id.gen, data.after.owner, data.after.gen,
                         "TODO SHAPE");
    }
    std::string operator()(const Delete &data) {
      return std::format("Delete ({}, {}) by ({}, {})", data.target.owner,
                         data.target.gen, data.by.owner, data.by.gen);
    }
    std::string operator()(const Hi &data) {
      return std::format("Hi {}", data.ask);
    }
    std::string operator()(const Ih &data) {
      return std::format("Ih {}, {}", data.permit, data.assign);
    }
  } vtor;

  return std::visit(vtor, data);
}

} // namespace pboard::net
