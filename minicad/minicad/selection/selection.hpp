#pragma once

#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <minicad/cursor/cursor.hpp>
#include <optional>
#include <unordered_set>

namespace mini {

class PointListObjectsSelection {
 public:
  void remove(const PointListObjectHandle& handle) {
    auto f = objs_.find(handle);
    if (f != objs_.end()) {
      objs_.erase(f);
    }
  }
  void add(const PointListObjectHandle& handle) { objs_.insert(handle); }
  void clear() { objs_.clear(); }

  bool contains(const PointListObjectHandle& handle) const { return objs_.contains(handle); }
  bool is_multi_selection() const { return objs_.size() > 1; }
  bool is_single_selection() const { return objs_.size() == 1; }
  bool is_empty() const { return objs_.empty(); }

  std::optional<PointListObjectHandle> single() {
    if (is_single_selection()) {
      return *objs_.begin();
    }
    return std::nullopt;
  }

  const PointListObjectHandle& first() { return *objs_.begin(); }

  auto begin() { return objs_.begin(); }
  auto begin() const { return objs_.begin(); }
  auto end() { return objs_.end(); }
  auto end() const { return objs_.end(); }

 private:
  std::unordered_set<PointListObjectHandle> objs_;
};

class SceneObjectsSelection {
 public:
  void remove(Scene& scene, const SceneObjectHandle& handle);
  void add(Scene& scene, const SceneObjectHandle& handle);
  void clear(Scene& scene);

  template <std::input_iterator Iterator>
  void add_many(Scene& scene, Iterator begin, Iterator end) {
    detach_all(scene);
    objs_.insert(begin, end);
    update_centroid(scene);
  }

  template <std::input_iterator Iterator>
  void remove_many(Scene& scene, Iterator begin, Iterator end) {
    detach_all(scene);
    for (const auto& handle : std::ranges::subrange(begin, end)) {
      auto f = objs_.find(handle);
      if (f != objs_.end()) {
        objs_.erase(begin, end);
      }
    }
    update_centroid(scene);
  }

  bool is_multi_selection() const { return objs_.size() > 1; }
  bool is_single_selection() const { return objs_.size() == 1; }
  bool is_empty() const { return objs_.empty(); }

  bool contains(const SceneObjectHandle& handle) const { return objs_.contains(handle); }

  auto centroid() const { return centroid_; }

  const SceneObjectHandle& first() { return *objs_.begin(); }

  void update_transforms(Scene& scene, Cursor& cursor);

  void set_custom_origin(Scene& scene, eray::math::Vec3f vec);
  void use_custom_origin(Scene& scene, bool use_custom_origin);
  bool is_using_custom_origin() const { return use_custom_origin_; }

  auto begin() { return objs_.begin(); }
  auto begin() const { return objs_.begin(); }
  auto end() { return objs_.end(); }
  auto end() const { return objs_.end(); }

 public:
  eray::math::Transform3f transform;

 private:
  void detach_all(Scene& scene);
  void update_centroid(Scene& scene);

 private:
  bool transform_dirty_;
  bool use_custom_origin_;

  eray::math::Vec3f centroid_;
  eray::math::Vec3f custom_origin_;

  std::unordered_set<SceneObjectHandle> objs_;
};

}  // namespace mini
