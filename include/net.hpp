#pragma once

#include "shape.hpp"
#include <asio.hpp>
#include <chrono>
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

  using Var = std::variant<Create, Hi, Ih, Delete, Fetch>;
  Var data;

  void serialize(std::vector<std::uint8_t> &out);
  static std::optional<Message> deserialize(std::span<const std::uint8_t> buf);

  std::string to_string() const;
};

struct BufferedMessage {
  asio::ip::udp::endpoint sender;
  Message msg;
  ObjectId depends;
};

constexpr std::uint32_t WINDOW_MAX = 32;

struct VersionWindow {
  // [0, prefix) -> true.
  Generation prefix = 0;
  // [prefix, prefix + WINDOW_MAX) -> wip[gen - prefix]
  std::vector<bool> wip = std::vector(WINDOW_MAX, false);
  // [prefix, prefix + WINDOW_MAX) -> received[gen - prefix]
  std::vector<bool> received = std::vector(WINDOW_MAX, false);
  // [prefix + WINDOW_MAX, ..) -> dropped

  // gens below prefix + cap, cap in [0, WINDOW_MAX]
  Generation cap = 0;

  void compact();
};

struct Timer {
  std::chrono::steady_clock::time_point expires_at;
  bool active = false;

  Timer() = default;

  Timer(std::chrono::steady_clock::duration d)
      : expires_at{std::chrono::steady_clock::now() + d}, active{true} {}

  bool expired() const {
    return std::chrono::steady_clock::now() >= expires_at;
  }
};

struct ClientState {
  asio::io_context &io_context;
  asio::ip::udp::socket sock;

  bool is_host;
  std::vector<asio::ip::udp::endpoint> host_eps{};

  ClientId self{}, asking{};
  Generation gen{0};
  std::flat_map<ClientId, VersionWindow> versions{};
  std::flat_map<ObjectId, Timer> last_requested{};

  std::flat_map<ObjectId, Object> objs{};
  std::flat_map<ObjectId, Message> messages{};
  ObjectId head{OBJ_NONE};
  ObjectId last{OBJ_NONE};

  std::vector<BufferedMessage> buffer{};

  void push_back(shape::Shape shape, ObjectId origin);
  void insert(ObjectId id, Object obj, ObjectId origin);
  void erase(ObjectId id);
  void flush_buffer();
  void request_all();

  void intro();
  void update();
  void handle(asio::ip::udp::endpoint sender, Message msg);

  template <bool IsConst> class Iterator {
  public:
    using iterator_category = std::bidirectional_iterator_tag;
    using iterator_concept = std::bidirectional_iterator_tag;
    using value_type = std::pair<ObjectId, Object>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference =
        std::pair<ObjectId,
                  std::conditional_t<IsConst, const Object &, Object &>>;

    Iterator() = default;

    Iterator(
        std::conditional_t<IsConst, const ClientState *, ClientState *> state,
        ObjectId id, bool forward = true)
        : state_(state), id_(id) {
      skip_deleted(forward);
    }

    reference operator*() const {
      return {id_,
              const_cast<std::conditional_t<IsConst, const Object &, Object &>>(
                  state_->objs.at(id_))};
    }

    Iterator &operator++() {
      id_ = (id_ != OBJ_NONE) ? state_->objs.at(id_).next : OBJ_NONE;
      skip_deleted(true);
      return *this;
    }

    Iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    Iterator &operator--() {
      id_ = (id_ == OBJ_NONE) ? state_->last : state_->objs.at(id_).prev;
      skip_deleted(false);
      return *this;
    }

    Iterator operator--(int) {
      auto tmp = *this;
      --(*this);
      return tmp;
    }

    friend bool operator==(const Iterator &, const Iterator &) = default;

  private:
    std::conditional_t<IsConst, const ClientState *, ClientState *> state_;
    ObjectId id_;

    void skip_deleted(bool forward) {
      while (id_ != OBJ_NONE) {
        auto it = state_->objs.find(id_);
        if (it == state_->objs.end() || it->second.deleted_by == OBJ_NONE)
          break;
        id_ = forward ? it->second.next : it->second.prev;
      }
    }
  };

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  iterator begin() { return {this, head, true}; }
  iterator end() { return {this, OBJ_NONE, true}; }
  const_iterator begin() const { return {this, head, true}; }
  const_iterator end() const { return {this, OBJ_NONE, true}; }

  iterator rbegin() { return {this, last, false}; }
  iterator rend() { return {this, OBJ_NONE, false}; }
  const_iterator rbegin() const { return {this, last, false}; }
  const_iterator rend() const { return {this, OBJ_NONE, false}; }

private:
  bool is_descendant(ObjectId origin, ObjectId candidate);
};

} // namespace pboard::net
