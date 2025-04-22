#pragma once

#include <cstdint>
#include <functional>
#include <liberay/math/transform3.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/zstring_view.hpp>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace mini {

using SceneObjectId     = std::uint32_t;
using PointListObjectId = std::uint32_t;

class Scene;

class SceneObject;
using SceneObjectHandle = eray::util::Handle<SceneObject>;

class PointListObject;
using PointListObjectHandle = eray::util::Handle<PointListObject>;

template <typename T>
concept CObjectHandle = std::is_same_v<T, SceneObjectHandle> || std::is_same_v<T, PointListObjectHandle>;

using ObjectHandle = std::variant<SceneObjectHandle, PointListObjectHandle>;

class PointListObject;

class Point {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Point"; }
};

class Torus {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Torus"; }

 public:
  float minor_radius           = 1.F;
  float major_radius           = 2.F;
  eray::math::Vec2i tess_level = eray::math::Vec2i(16, 16);
};

using SceneObjectVariant = std::variant<Point, Torus>;

class SceneObject {
 public:
  SceneObject() = delete;
  explicit SceneObject(SceneObjectHandle handle, Scene& scene) : handle_(handle), scene_(scene) {}
  ~SceneObject();

  SceneObjectId id() const { return handle_.obj_id; }
  const SceneObjectHandle& handle() const { return handle_; }

  void mark_dirty();
  size_t order_ind() const { return order_ind_; }

 public:
  eray::math::Transform3f transform;
  std::string name;
  SceneObjectVariant object;

 private:
  friend Scene;
  friend PointListObject;

  size_t order_ind_{0};

  std::unordered_set<PointListObjectHandle> point_lists_;

  SceneObjectHandle handle_;
  Scene& scene_;  // NOLINT
};

class Polyline {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Polyline"; }
};

class MultisegmentBezierCurve {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Multisegment Bezier Curve"; }
};

using PointListObjectVariant = std::variant<Polyline, MultisegmentBezierCurve>;

class PointListObject {
 public:
  PointListObject() = delete;
  explicit PointListObject(PointListObjectHandle handle, Scene& scene) : handle_(handle), scene_(scene) {}
  ~PointListObject();

  auto points() {
    return points_ | std::ranges::views::transform([](auto& ref) -> auto& { return ref.get(); });
  }

  auto points() const {
    return points_ | std::ranges::views::transform([](const auto& ref) -> const auto& { return ref.get(); });
  }

  auto handles() { return points_map_ | std::views::keys; }

  bool contains(const SceneObjectHandle& handle) { return points_map_.contains(handle); }

  enum class SceneObjectError : uint8_t {
    NotAPoint        = 0,
    HandleIsNotValid = 1,
    NotFound         = 2,
  };

  std::expected<void, SceneObjectError> add(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> remove(const SceneObjectHandle& handle);
  std::expected<void, SceneObjectError> move_before(const SceneObjectHandle& dest, const SceneObjectHandle& obj);
  std::expected<void, SceneObjectError> move_after(const SceneObjectHandle& dest, const SceneObjectHandle& obj);

  PointListObjectId id() const { return handle_.obj_id; }
  const PointListObjectHandle& handle() const { return handle_; }
  void mark_dirty();

  size_t order_ind() const { return order_ind_; }

 private:
  void update_indices_from(size_t start_idx);

 public:
  std::string name;
  PointListObjectVariant object;

 private:
  friend Scene;
  friend SceneObject;

  PointListObjectHandle handle_;
  Scene& scene_;  // NOLINT

  size_t order_ind_{0};

  std::vector<std::reference_wrapper<SceneObject>> points_;
  std::unordered_map<SceneObjectHandle, size_t> points_map_;
};

}  // namespace mini
