#include "net.hpp"
#include "protocol.hpp"
#include "shape.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <flat_set>
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

      if (data.id.gen < ver.prefix)
        return;

      if (data.id.gen >= ver.prefix + WINDOW_MAX) {
        std::println("Creation past the window?");
        self.synchronise(sender);
        return;
      }

      if (data.id.gen >= ver.prefix && data.id.gen < ver.prefix + WINDOW_MAX) {
        ver.cap = std::max(ver.cap, data.id.gen - ver.prefix + 1);
        ver.received[data.id.gen - ver.prefix] = true;
      }

      if (self.objs.contains(data.after) || data.after == OBJ_NONE) {
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

      if (data.by.gen < ver.prefix)
        return;

      if (data.by.gen >= ver.prefix + WINDOW_MAX) {
        std::println("Deletion past the window?");
        self.synchronise(sender);
        return;
      }

      if (data.by.gen >= ver.prefix && data.by.gen < ver.prefix + WINDOW_MAX) {
        ver.cap = std::max(ver.cap, data.by.gen - ver.prefix + 1);
        ver.received[data.by.gen - ver.prefix] = true;
      }

      if (auto it = self.objs.find(data.target); it != self.objs.end()) {
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
        self.synchronise(sender);
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
      auto peer_ver = self.peer_versions.find(data.sender);
      if (peer_ver != self.peer_versions.end() &&
          peer_ver->second.last.expired()) {
        // TODO: Stale client?
        std::println("Stale client reconnected {}", data.sender);
        return;
      }

      using namespace std::chrono_literals;
      self.peer_versions[data.sender].last = Timer{10s};
      self.peer_versions[data.sender].certain = data.certain;

      auto &ver = self.versions[data.sender];
      if (data.gen >= ver.prefix && data.gen <= ver.prefix + WINDOW_MAX) {
        ver.cap = std::max(ver.cap, data.gen - ver.prefix);
      } else {
        self.synchronise(sender);
        return;
      }

      for (const auto &[owner, gen] : data.certain) {
        if (owner == self.self)
          continue;
        auto &ver = self.versions[owner];
        if (gen >= ver.prefix && gen <= ver.prefix + WINDOW_MAX) {
          ver.cap = std::max(ver.cap, gen - ver.prefix);
        } else {
          self.synchronise(sender);
          return;
        }
      }
    }

    void operator()(const Message::BulkRequest &data) {
      auto &s = self.sync_with[data.from];

      if (s.active)
        return;

      s.active = true;
      s.to = sender;
      s.sent = 0;
      s.frozen.clear();

      for (const auto &[id, obj] : self.objs) {
        auto it = data.certain.find(id.owner);
        if (it == data.certain.end() || id.gen >= it->second) {
          s.frozen.push_back(
              {id, ObjectData{obj.shape, obj.origin, obj.deleted_by}});
        }
      }
    }

    void operator()(const Message::BulkResponse &data) {
      if (self.sync.active && self.sync.len == 0) {
        self.sync.len = data.total;
        self.sync.chunks.resize(data.total);
        self.sync.have.resize(data.total * BULK_SIZE);
      }

      if (data.index >= self.sync.len)
        return;

      bool last_chunk = (data.index == data.total - 1);
      if (last_chunk) {
        if (data.bulk.empty() || data.bulk.size() > BULK_SIZE)
          return;
      } else {
        if (data.bulk.size() != BULK_SIZE)
          return;
      }

      if (self.sync.chunks[data.index])
        return;

      self.sync.chunks[data.index] = true;

      std::copy(data.bulk.begin(), data.bulk.end(),
                self.sync.have.begin() + (data.index * BULK_SIZE));

      if (last_chunk && data.bulk.size() < BULK_SIZE) {
        self.sync.have.resize((data.total - 1) * BULK_SIZE + data.bulk.size());
      }

      using namespace std::chrono_literals;
      self.sync.last = Timer{1s};
    }

    void operator()(const Message::BulkNAck &data) {
      auto &sync = self.sync_with[data.from];
      if (!sync.active)
        return;
      if (data.index >= sync.len())
        return;
      auto remain = std::min(static_cast<std::size_t>(BULK_SIZE),
                             sync.frozen.size() - BULK_SIZE * data.index);
      std::vector<std::pair<ObjectId, ObjectData>> bulk{};
      bulk.resize(remain);
      std::copy_n(sync.frozen.begin() + BULK_SIZE * data.index, remain,
                  bulk.begin());
      Message msg{Message::BulkResponse{
          data.index, self.sync_with[data.from].len(), std::move(bulk)}};
      self.send_to(sender, std::move(msg));
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

  if (!sync.active) {
    request_all();
  } else {
    check_sync();
  }

  sync_peers();
  cleanup();
}

void ClientState::cleanup() {
  std::flat_map<ClientId, Generation> consensus{};

  consensus[self] = gen;

  for (const auto &[peer, ver] : versions) {
    consensus[peer] = ver.prefix;
  }

  for (const auto &[owner, vers] : peer_versions) {
    if (vers.last.expired())
      continue;
    for (const auto &[peer, ver] : vers.certain) {
      consensus[peer] = std::min(consensus[peer], ver);
    }
  }

  for (auto it = messages.begin(); it != messages.end();) {
    if (it->first.gen < consensus[it->first.owner]) {
      std::println("LOG: Clear message ({}, {})", it->first.owner,
                   it->first.gen);
      messages.erase(it);
    } else {
      ++it;
    }
  }

  std::flat_set<ObjectId> removed;

  for (const auto &[id, obj] : objs) {
    if (obj.deleted_by != OBJ_NONE &&
        obj.deleted_by.gen < consensus[obj.deleted_by.owner] &&
        id.gen < consensus[id.owner]) {
      removed.emplace(id);
    }
  }

  auto resolve_prev = [&](ObjectId id) {
    while (id != OBJ_NONE && removed.contains(id)) {
      auto it = objs.find(id);
      if (it == objs.end())
        return OBJ_NONE;
      id = it->second.prev;
    }
    return id;
  };

  auto resolve_next = [&](ObjectId id) {
    while (id != OBJ_NONE && removed.contains(id)) {
      auto it = objs.find(id);
      if (it == objs.end())
        return OBJ_NONE;
      id = it->second.next;
    }
    return id;
  };

  for (const auto &[id, obj] : objs) {
    if (removed.contains(id))
      continue;

    obj.prev = resolve_prev(obj.prev);
    obj.next = resolve_next(obj.next);

    while (obj.origin != OBJ_NONE && removed.contains(obj.origin)) {
      auto it = objs.find(obj.origin);
      if (it == objs.end()) {
        obj.origin = OBJ_NONE;
        break;
      }
      obj.origin = it->second.origin;
    }
  }

  head = resolve_next(head);
  last = resolve_prev(last);

  for (ObjectId id : removed) {
    std::println("LOG: Clear object ({}, {})", id.owner, id.gen);
    objs.erase(id);
  }
}

void ClientState::synchronise(asio::ip::udp::endpoint from) {
  if (sync.active)
    return;

  std::flat_map<ClientId, Generation> certain{};
  for (const auto &[owner, ver] : versions) {
    certain[owner] = ver.prefix;
  }
  Message msg{Message::BulkRequest{self, std::move(certain)}};
  send_to(from, std::move(msg));

  using namespace std::chrono_literals;
  buffer.clear();
  last_requested.clear();
  sync = {.active = true,
          .peer = from,
          .len = 0,
          .have = {},
          .chunks = {},
          .last = Timer{3s}};
}

void ClientState::check_sync() {
  if (sync.len == 0) {
    if (sync.last.expired()) {
      synchronise(sync.peer);
    }
    return;
  }

  bool have_all{true};

  for (std::uint32_t i = 0; i < sync.len; ++i) {
    if (sync.chunks[i]) {
      continue;
    }

    have_all = false;
    if (sync.last.expired()) {
      Message msg{Message::BulkNAck{self, i}};
      send_to(sync.peer, msg);
      using namespace std::chrono_literals;
      sync.last = Timer{1s};
    }
  }

  if (have_all) {
    std::flat_map<ObjectId, ObjectData> remaining;
    for (const auto &[id, data] : sync.have) {
      remaining[id] = data;
    }
    std::vector<std::pair<ObjectId, ObjectData>> ordered;
    ordered.reserve(remaining.size());
    bool progress = true;
    while (!remaining.empty() && progress) {
      progress = false;
      for (auto it = remaining.begin(); it != remaining.end();) {
        const auto &[id, data] = *it;

        bool satisfied =
            (data.origin == OBJ_NONE) || (objs.contains(data.origin)) ||
            (std::find_if(ordered.begin(), ordered.end(), [&data](auto x) {
               return x.first == data.origin;
             }) != ordered.end());

        if (satisfied) {
          ordered.push_back({id, data});
          remaining.erase(it);
          it = remaining.begin();
          progress = true;
        } else {
          ++it;
        }
      }
    }

    if (!remaining.empty()) {
      // TODO: Fetch these?
      std::println("LOG: Sync failed");
      for (const auto &[id, data] : remaining) {
        std::println("  Missing: ({}, {}), waiting on origin ({}, {})",
                     id.owner, id.gen, data.origin.owner, data.origin.gen);
      }
      sync = {};
      return;
    }

    for (const auto &[id, data] : ordered) {
      if (!objs.contains(id)) {
        insert(id,
               Object{data.shape, data.origin, OBJ_NONE, OBJ_NONE,
                      data.deleted_by},
               data.origin);
      }
      // TODO: Error prone?
      auto &ver = versions[id.owner];
      ver.prefix = std::max(ver.prefix, id.gen);
      std::fill(ver.wip.begin(), ver.wip.end(), false);
      std::fill(ver.received.begin(), ver.received.end(), false);
      ver.cap = 0;
    }

    sync = {};
  }
}

void ClientState::sync_peers() {
  for (const auto &[id, sync] : sync_with) {
    if (!sync.active)
      continue;
    if (sync.sent >= sync.len()) {
      sync.active = false;
      continue;
    }

    auto remain = std::min(static_cast<std::size_t>(BULK_SIZE),
                           sync.frozen.size() - sync.sent * BULK_SIZE);

    std::vector<std::pair<ObjectId, ObjectData>> bulk{};
    bulk.resize(remain);
    std::copy_n(sync.frozen.begin() + sync.sent * BULK_SIZE, remain,
                bulk.begin());
    Message msg{Message::BulkResponse{sync.sent, sync.len(), std::move(bulk)}};
    send_to(sync.to, std::move(msg));
    sync.sent++;
  }
}

std::uint32_t SyncState::len() {
  return (frozen.size() + (BULK_SIZE - 1)) / BULK_SIZE;
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
