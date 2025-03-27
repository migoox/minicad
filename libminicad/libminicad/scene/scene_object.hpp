#pragma once

#include <cstdint>
#include <liberay/math/transform3.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/zstring_view.hpp>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace mini {

using SceneObjectId     = std::uint32_t;
using PointListObjectId = std::uint32_t;

class Scene;

class SceneObject;
using SceneObjectHandle = eray::util::Handle<SceneObject>;

class Point;
using PointHandle = eray::util::Handle<Point>;

class PointListObject;
using PointListObjectHandle = eray::util::Handle<PointListObject>;

using AnyObjectHandle = eray::util::AnyObjectHandle;

class Point {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Point"; }

  const std::unordered_set<PointListObjectHandle>& point_lists() const { return point_lists_; }

 private:
  friend Scene;

  std::unordered_set<PointListObjectHandle> point_lists_;
};

class Torus {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Torus"; }

 public:
  float minor_radius;
  float major_radius;
};

using SceneObjectVariant = std::variant<Point, Torus>;

class SceneObject {
 public:
  SceneObject() = delete;
  explicit SceneObject(SceneObjectId id) : id_(id) {}

  SceneObjectId id() const { return id_; }

 public:
  eray::math::Transform3f transform;
  std::string name;
  SceneObjectVariant object;

 private:
  friend Scene;

  SceneObjectId id_;
};

class PointListObject {
 public:
  PointListObject() = delete;
  explicit PointListObject(PointListObjectId id) : id_(id) {}

  const std::list<PointHandle>& points() { return points_; }
  bool contains(const PointHandle& handle) { return points_map_.contains(handle); }
  bool contains(const SceneObjectHandle& handle) {
    return points_map_.contains(PointHandle(handle.owner_signature, handle.timestamp, handle.obj_id));
  }

  PointListObjectId id() const { return id_; }

 public:
  std::string name;

 private:
  friend Scene;

  PointListObjectId id_;

  std::list<PointHandle> points_;
  std::unordered_map<PointHandle, std::list<PointHandle>::iterator> points_map_;
};

}  // namespace mini
