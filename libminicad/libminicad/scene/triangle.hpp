#pragma once

#include <cstdint>
#include <liberay/util/hash_combine.hpp>
#include <utility>

namespace mini {

struct Triangle {
  std::uint32_t obj_id1;
  std::uint32_t obj_id2;
  std::uint32_t obj_id3;

  std::uint32_t obj_id12;
  std::uint32_t obj_id22;
  std::uint32_t obj_id32;

  static Triangle create(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t a2, std::uint32_t b2,
                         std::uint32_t c2) {
    if (a > b) {
      std::swap(a, b);
    }
    if (b > c) {
      std::swap(b, c);
    }
    if (a > b) {
      std::swap(a, b);
    }

    if (a2 > b2) {
      std::swap(a2, b2);
    }
    if (b2 > c2) {
      std::swap(b2, c2);
    }
    if (a2 > b2) {
      std::swap(a2, b2);
    }

    return Triangle{.obj_id1 = a, .obj_id2 = b, .obj_id3 = c, .obj_id12 = a2, .obj_id22 = b2, .obj_id32 = c2};
  }

  friend bool operator==(const Triangle& lh, const Triangle& rh) {
    return lh.obj_id1 == rh.obj_id1 && lh.obj_id2 == rh.obj_id2 && lh.obj_id3 == rh.obj_id3 &&
           lh.obj_id12 == rh.obj_id12 && lh.obj_id22 == rh.obj_id22 && lh.obj_id32 == rh.obj_id32;
  }
};

}  // namespace mini

namespace std {

template <>
struct hash<mini::Triangle> {
  size_t operator()(const mini::Triangle& triangle) const noexcept {
    auto h1 = static_cast<size_t>(triangle.obj_id1);
    auto h2 = static_cast<size_t>(triangle.obj_id2);
    auto h3 = static_cast<size_t>(triangle.obj_id3);
    auto h4 = static_cast<size_t>(triangle.obj_id12);
    auto h5 = static_cast<size_t>(triangle.obj_id22);
    auto h6 = static_cast<size_t>(triangle.obj_id32);
    eray::util::hash_combine(h1, h2);
    eray::util::hash_combine(h1, h3);
    eray::util::hash_combine(h1, h4);
    eray::util::hash_combine(h1, h5);
    eray::util::hash_combine(h1, h6);
    return h1;
  }
};

}  // namespace std
