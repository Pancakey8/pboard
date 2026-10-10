#include "net.hpp"

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
}

void ClientState::push_back(shape::Shape shape, ObjectId origin) {
  ObjectId id{self, gen++};
  Object obj{shape, origin, OBJ_NONE, OBJ_NONE};

  insert(id, obj, origin);
}

void ClientState::erase(ObjectId id) {
  if (auto it = objs.find(id); it != objs.end()) {
    it->second.deleted = true;
  }
}

} // namespace pboard::net
