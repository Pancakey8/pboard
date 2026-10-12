#include "net.hpp"
#include "shape.hpp"
#include <chrono>
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
  messages[id] = msg;
  send_all(std::move(msg));

  insert(id, obj, origin);
}

void ClientState::erase(ObjectId id) {
  if (auto it = objs.find(id); it != objs.end()) {
    ObjectId by{self, gen++};
    it->second.deleted_by = by;
    Message msg{Message::Delete{by, id}};
    messages[by] = msg;
    send_all(std::move(msg));

    flush_buffer();
  }
}

void ClientState::flush_buffer() {
  bool progress{false};
  do {
    progress = false;
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

void ClientState::request_all() {
  for (const auto &[owner, ver] : versions) {
    if (ver.cap == 0)
      continue;

    for (std::uint32_t i = 0; i < ver.cap; ++i) {
      if (ver.received[i])
        continue;

      ObjectId id = {owner, ver.prefix + i};
      if (!last_requested[id].active || last_requested[id].expired()) {
        using namespace std::chrono_literals;
        last_requested[id] = Timer{3s};
        Message msg{Message::Fetch{id}};
        send_all(std::move(msg));
        std::println("LOG: Requesting ({}, {})", id.owner, id.gen);
      }
    }
  }
}

void ClientState::intro() {
  if (is_host) {
    self = uuids::uuid_system_generator{}();
  } else {
    asking = uuids::uuid_system_generator{}();
    Message msg{Message::Hi{asking}};
    // TODO: Rethink for p2p
    send_all(std::move(msg));
  }
}

void ClientState::handle(asio::ip::udp::endpoint sender, Message msg) {
  struct {
    asio::ip::udp::endpoint sender;
    ClientState &self;

    void operator()(const Message::Create &data) {
      self.messages[data.id] = {data};
      auto &ver = self.versions[data.id.owner];

      if (data.id.gen >= ver.prefix && data.id.gen < ver.prefix + WINDOW_MAX) {
        ver.cap = std::max(ver.cap, data.id.gen - ver.prefix + 1);
        ver.received[data.id.gen - ver.prefix] = true;
      }

      if (self.objs.contains(data.after) || data.after == OBJ_NONE) {
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
      self.messages[data.by] = {data};
      auto &ver = self.versions[data.by.owner];

      if (data.by.gen >= ver.prefix && data.by.gen < ver.prefix + WINDOW_MAX) {
        ver.cap = std::max(ver.cap, data.by.gen - ver.prefix + 1);
        ver.received[data.by.gen - ver.prefix] = true;
      }

      if (auto it = self.objs.find(data.target); it != self.objs.end()) {
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
        self.send_to(sender, std::move(msg));
      }
    }

    void operator()(const Message::Ih &data) {
      bool asked_it = data.assign == self.asking;
      if (!self.is_host && asked_it && data.permit) {
        self.self = data.assign;
      }
      self.asking = {};
    }

    void operator()(const Message::Fetch &data) {
      std::println("Someone fetched ({}, {})", data.id.owner, data.id.gen);

      if (auto it = self.messages.find(data.id); it != self.messages.end()) {
        std::vector<std::uint8_t> buf{};
        auto resp_msg = it->second;
        // TODO: Should be send_to?
        self.send_all(std::move(resp_msg));
      }
    }

    void operator()(const Message::Versions &data) {
      // TODO: Use for GC
      auto &ver = self.versions[data.sender];
      if (data.gen >= ver.prefix && data.gen <= ver.prefix + WINDOW_MAX) {
        ver.cap = std::max(ver.cap, data.gen - ver.prefix);
      }

      for (const auto &[owner, gen] : data.certain) {
        if (owner == self.self)
          continue;
        auto &ver = self.versions[owner];
        if (gen >= ver.prefix && gen <= ver.prefix + WINDOW_MAX) {
          ver.cap = std::max(ver.cap, gen - ver.prefix);
        }
      }
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

  if (!self.is_nil() && (!version_timer.active || version_timer.expired())) {
    using namespace std::chrono_literals;
    version_timer = Timer{3s};
    std::flat_map<ClientId, Generation> certain{};
    for (const auto &[owner, ver] : versions) {
      certain[owner] = ver.prefix;
    }
    std::println("LOG: Exchange ({}, {}): {}", self, gen, certain);
    Message msg{Message::Versions{self, gen, std::move(certain)}};
    send_all(std::move(msg));
  }

  request_all();
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
  std::copy(received.begin() + slide, received.end(), received.begin());
  std::fill(received.end() - slide, received.end(), false);
  cap = (cap > slide) ? cap - slide : 0;
}

void ClientState::send_to(asio::ip::udp::endpoint target, Message msg) {
  std::vector<std::uint8_t> buf;
  msg.serialize(buf);
  asio::error_code ec;
  sock.send_to(asio::buffer(buf), target, 0, ec);
  if (ec) {
    std::println("LOG: Failed to send to target");
  }
}

void ClientState::send_all(Message msg) {
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
}

} // namespace pboard::net
