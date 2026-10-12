#pragma once
#include "shape.hpp"
#include <compare>
#include <cstdint>
#include <flat_map>
#include <uuid.h>

namespace pboard::net {

using ClientId = uuids::uuid;
using Generation = std::uint32_t;

struct ObjectId {
  ClientId owner{};
  Generation gen{static_cast<Generation>(-1)};

  friend constexpr std::strong_ordering operator<=>(const ObjectId &lhs,
                                                    const ObjectId &rhs) {
    if (lhs.owner < rhs.owner)
      return std::strong_ordering::less;
    if (rhs.owner < lhs.owner)
      return std::strong_ordering::greater;

    if (lhs.gen < rhs.gen)
      return std::strong_ordering::less;
    if (rhs.gen < lhs.gen)
      return std::strong_ordering::greater;

    return std::strong_ordering::equal;
  }

  friend constexpr bool operator==(const ObjectId &,
                                   const ObjectId &) = default;
};

constexpr ObjectId OBJ_NONE{};

struct Object {
  shape::Shape shape;
  ObjectId origin;
  ObjectId prev, next;
  ObjectId deleted_by{OBJ_NONE};
};

struct Message {
  struct Create {
    ObjectId id;
    shape::Shape shape;
    ObjectId after;
  };

  struct Delete {
    ObjectId by;
    ObjectId target;
  };

  struct Hi {
    ClientId ask;
  };

  struct Ih {
    bool permit;
    ClientId assign;
  };

  struct Fetch {
    ObjectId id;
  };

  struct Versions {
    ClientId sender;
    Generation gen;
    std::flat_map<ClientId, Generation> certain{};
  };

  using Var = std::variant<Create, Hi, Ih, Delete, Fetch, Versions>;
  Var data;

  void serialize(std::vector<std::uint8_t> &out);
  static std::optional<Message> deserialize(std::span<const std::uint8_t> buf);

  std::string to_string() const;
};

}; // namespace pboard::net
