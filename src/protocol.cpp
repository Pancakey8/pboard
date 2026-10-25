#include "protocol.hpp"
#include "shape.hpp"
#include <algorithm>

namespace pboard::net {

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

    void operator()(const shape::Bezier &bez) {
      out.push_back('B');
      out.push_back('Z');
      serialize_num(bez.x0, out);
      serialize_num(bez.y0, out);
      serialize_num(bez.x1, out);
      serialize_num(bez.y1, out);
      serialize_num(bez.x2, out);
      serialize_num(bez.y2, out);
      serialize_num(bez.x3, out);
      serialize_num(bez.y3, out);
      serialize_num(bez.thick, out);
      serialize_num(bez.col.r, out);
      serialize_num(bez.col.g, out);
      serialize_num(bez.col.b, out);
    }
  } serial{out};

  std::visit(serial, shape);
}

void serialize_endpoint(const asio::ip::udp::endpoint &ep,
                        std::vector<std::uint8_t> &out) {
  auto addr = ep.address().to_v4().to_bytes();

  out.insert(out.end(), addr.begin(), addr.end());

  auto port = ep.port();
  out.push_back(static_cast<std::uint8_t>(port >> 8));
  out.push_back(static_cast<std::uint8_t>(port));
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
      serialize_id(data.id, out);
    }

    void operator()(const Ih &data) {
      out.push_back('I');
      out.push_back('H');
      serialize_id(data.id, out);
      serialize_num(static_cast<std::uint32_t>(data.eps.size()), out);
      for (const auto &[id, ep] : data.eps) {
        serialize_id(id, out);
        serialize_endpoint(ep, out);
      }
    }

    void operator()(const Delete &data) {
      out.push_back('D');
      out.push_back('L');
      serialize_id(data.by.owner, out);
      serialize_num(data.by.gen, out);
      serialize_id(data.target.owner, out);
      serialize_num(data.target.gen, out);
    }

    void operator()(const Fetch &data) {
      out.push_back('F');
      out.push_back('C');
      serialize_id(data.id.owner, out);
      serialize_num(data.id.gen, out);
    }

    void operator()(const Versions &data) {
      out.push_back('V');
      out.push_back('R');
      serialize_id(data.sender, out);
      serialize_num(data.gen, out);
      serialize_num(static_cast<std::uint32_t>(data.certain.size()), out);
      for (const auto &[owner, gen] : data.certain) {
        serialize_id(owner, out);
        serialize_num(gen, out);
      }
    }

    void operator()(const BulkHeader &data) {
      out.push_back('B');
      out.push_back('H');
      serialize_num(data.total, out);
      serialize_num(static_cast<std::uint32_t>(data.certain.size()), out);
      for (const auto &[owner, gen] : data.certain) {
        serialize_id(owner, out);
        serialize_num(gen, out);
      }
    }

    void operator()(const BulkRequest &data) {
      out.push_back('B');
      out.push_back('Q');
      serialize_id(data.from, out);
      serialize_num(static_cast<std::uint32_t>(data.certain.size()), out);
      for (const auto &[owner, gen] : data.certain) {
        serialize_id(owner, out);
        serialize_num(gen, out);
      }
    }

    void operator()(const BulkResponse &data) {
      out.push_back('B');
      out.push_back('A');
      serialize_num(data.index, out);
      serialize_num(data.total, out);
      serialize_num(static_cast<std::uint32_t>(data.bulk.size()), out);
      for (const auto &[id, obj] : data.bulk) {
        serialize_id(id.owner, out);
        serialize_num(id.gen, out);
        serialize_shape(obj.shape, out);
        serialize_id(obj.origin.owner, out);
        serialize_num(obj.origin.gen, out);
        serialize_id(obj.deleted_by.owner, out);
        serialize_num(obj.deleted_by.gen, out);
      }
    }

    void operator()(const BulkNAck &data) {
      out.push_back('B');
      out.push_back('N');
      serialize_id(data.from, out);
      serialize_num(data.index, out);
    }

    void operator()(const WhereIs &data) {
      out.push_back('W');
      out.push_back('Q');
      serialize_id(data.peer, out);
    }

    void operator()(const HereIs &data) {
      out.push_back('W');
      out.push_back('R');
      serialize_id(data.peer, out);
      serialize_endpoint(data.ep, out);
    }

    void operator()(const CursorAt &data) {
      out.push_back('K');
      out.push_back('S');
      serialize_id(data.id, out);
      serialize_num(data.x, out);
      serialize_num(data.y, out);
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
  } else if (tag1 == 'B' && tag2 == 'Z') {
    shape::Bezier bez;
    if (!deserialize_num(bez.x0, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.y0, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.x1, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.y1, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.x2, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.y2, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.x3, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.y3, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.thick, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.col.r, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.col.g, it, end))
      return std::nullopt;
    if (!deserialize_num(bez.col.b, it, end))
      return std::nullopt;
    return shape::Shape{bez};
  }

  return std::nullopt;
}

bool deserialize_endpoint(asio::ip::udp::endpoint &val, auto &it, auto end) {
  if (std::distance(it, end) < 6) {
    return false;
  }

  asio::ip::address_v4::bytes_type addr;
  std::copy_n(it, 4, addr.begin());
  it += 4;

  std::uint16_t port = (static_cast<std::uint16_t>(it[0]) << 8) |
                       static_cast<std::uint16_t>(it[1]);
  it += 2;

  val = asio::ip::udp::endpoint{asio::ip::address_v4{addr}, port};
  return true;
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
    if (!deserialize_id(data.id, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'I' && tag2 == 'H') {
    Message::Ih data;
    if (!deserialize_id(data.id, it, end))
      return std::nullopt;
    std::uint32_t len;
    if (!deserialize_num(len, it, end))
      return std::nullopt;
    auto cont = std::move(data.eps).extract();
    cont.keys.reserve(len);
    cont.values.reserve(len);
    data.eps.replace(std::move(cont.keys), std::move(cont.values));
    for (std::uint32_t i = 0; i < len; ++i) {
      ClientId id;
      asio::ip::udp::endpoint ep;
      if (!deserialize_id(id, it, end))
        return std::nullopt;
      if (!deserialize_endpoint(ep, it, end))
        return std::nullopt;
      data.eps[id] = ep;
    }
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
  } else if (tag1 == 'F' && tag2 == 'C') {
    Message::Fetch data;
    if (!deserialize_id(data.id.owner, it, end))
      return std::nullopt;
    if (!deserialize_num(data.id.gen, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'V' && tag2 == 'R') {
    Message::Versions data;
    if (!deserialize_id(data.sender, it, end))
      return std::nullopt;
    if (!deserialize_num(data.gen, it, end))
      return std::nullopt;
    std::uint32_t len;
    if (!deserialize_num(len, it, end))
      return std::nullopt;
    auto cont = std::move(data.certain).extract();
    cont.keys.reserve(len);
    cont.values.reserve(len);
    data.certain.replace(std::move(cont.keys), std::move(cont.values));
    for (std::uint32_t i = 0; i < len; ++i) {
      ClientId owner;
      Generation gen;
      if (!deserialize_id(owner, it, end))
        return std::nullopt;
      if (!deserialize_num(gen, it, end))
        return std::nullopt;
      data.certain[owner] = gen;
    }
    return Message::Var{data};
  } else if (tag1 == 'B' && tag2 == 'H') {
    Message::BulkHeader data;
    if (!deserialize_num(data.total, it, end))
      return std::nullopt;
    std::uint32_t len;
    if (!deserialize_num(len, it, end))
      return std::nullopt;
    auto cont = std::move(data.certain).extract();
    cont.keys.reserve(len);
    cont.values.reserve(len);
    data.certain.replace(std::move(cont.keys), std::move(cont.values));
    for (std::uint32_t i = 0; i < len; ++i) {
      ClientId owner;
      Generation gen;
      if (!deserialize_id(owner, it, end))
        return std::nullopt;
      if (!deserialize_num(gen, it, end))
        return std::nullopt;
      data.certain[owner] = gen;
    }
    return Message::Var{data};
  } else if (tag1 == 'B' && tag2 == 'Q') {
    Message::BulkRequest data;
    if (!deserialize_id(data.from, it, end))
      return std::nullopt;
    std::uint32_t len;
    if (!deserialize_num(len, it, end))
      return std::nullopt;
    auto cont = std::move(data.certain).extract();
    cont.keys.reserve(len);
    cont.values.reserve(len);
    data.certain.replace(std::move(cont.keys), std::move(cont.values));
    for (std::uint32_t i = 0; i < len; ++i) {
      ClientId owner;
      Generation gen;
      if (!deserialize_id(owner, it, end))
        return std::nullopt;
      if (!deserialize_num(gen, it, end))
        return std::nullopt;
      data.certain[owner] = gen;
    }
    return Message::Var{data};
  } else if (tag1 == 'B' && tag2 == 'A') {
    Message::BulkResponse data;
    if (!deserialize_num(data.index, it, end))
      return std::nullopt;
    if (!deserialize_num(data.total, it, end))
      return std::nullopt;
    std::uint32_t len;
    if (!deserialize_num(len, it, end))
      return std::nullopt;
    data.bulk.reserve(len);
    for (std::uint32_t i = 0; i < len; ++i) {
      ObjectId id;
      ObjectData obj;
      if (!deserialize_id(id.owner, it, end))
        return std::nullopt;
      if (!deserialize_num(id.gen, it, end))
        return std::nullopt;
      auto shape_opt = deserialize_shape(it, end);
      if (!shape_opt)
        return std::nullopt;
      obj.shape = *shape_opt;
      if (!deserialize_id(obj.origin.owner, it, end))
        return std::nullopt;
      if (!deserialize_num(obj.origin.gen, it, end))
        return std::nullopt;
      if (!deserialize_id(obj.deleted_by.owner, it, end))
        return std::nullopt;
      if (!deserialize_num(obj.deleted_by.gen, it, end))
        return std::nullopt;
      data.bulk.push_back({id, std::move(obj)});
    }
    return Message::Var{data};
  } else if (tag1 == 'B' && tag2 == 'N') {
    Message::BulkNAck data;
    if (!deserialize_id(data.from, it, end))
      return std::nullopt;
    if (!deserialize_num(data.index, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'W' && tag2 == 'Q') {
    Message::WhereIs data;
    if (!deserialize_id(data.peer, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'W' && tag2 == 'R') {
    Message::HereIs data;
    if (!deserialize_id(data.peer, it, end))
      return std::nullopt;
    if (!deserialize_endpoint(data.ep, it, end))
      return std::nullopt;
    return Message::Var{data};
  } else if (tag1 == 'K' && tag2 == 'S') {
    Message::CursorAt data;
    if (!deserialize_id(data.id, it, end))
      return std::nullopt;
    if (!deserialize_num(data.x, it, end))
      return std::nullopt;
    if (!deserialize_num(data.y, it, end))
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
      return std::format("Hi {}", data.id);
    }
    std::string operator()(const Ih &data) {
      std::vector<std::string> eps{};
      for (const auto &[id, ep] : data.eps) {
        eps.push_back(
            std::format("{}, {}:{}", id, ep.address().to_string(), ep.port()));
      }
      return std::format("Ih {}: {}", data.id, eps);
    }
    std::string operator()(const Fetch &data) {
      return std::format("Fetch ({}, {})", data.id.owner, data.id.gen);
    }
    std::string operator()(const Versions &data) {
      return std::format("Versions ({}, {}): {}", data.sender, data.gen,
                         data.certain);
    }
    std::string operator()(const BulkHeader &data) {
      return std::format("BulkHeader -/{}: {}", data.total, data.certain);
    }
    std::string operator()(const BulkRequest &data) {
      return std::format("BulkRequest {}: {}", data.from, data.certain);
    }
    std::string operator()(const BulkResponse &data) {
      std::vector<std::pair<std::string, std::string>> bulk_str{};
      for (const auto &[id, obj] : data.bulk) {
        bulk_str.push_back(
            {std::format("({}, {})", id.owner, id.gen),
             std::format("TODO SHAPE at ({}, {}) / Deleted: ({}, {})",
                         obj.origin.owner, obj.origin.gen, obj.deleted_by.owner,
                         obj.deleted_by.gen)});
      }
      return std::format("BulkResponse {}/{}: {}", data.index, data.total,
                         bulk_str);
    }
    std::string operator()(const BulkNAck &data) {
      return std::format("BulkNAck {}: {}", data.from, data.index);
    }
    std::string operator()(const WhereIs &data) {
      return std::format("WhereIs {}", data.peer);
    }
    std::string operator()(const HereIs &data) {
      return std::format("HereIs {}, {}:{}", data.peer,
                         data.ep.address().to_string(), data.ep.port());
    }
    std::string operator()(const CursorAt &data) {
      return std::format("CursorAt {}: [{}, {}]", data.id, data.x, data.y);
    }
  } vtor;

  return std::visit(vtor, data);
}

} // namespace pboard::net
