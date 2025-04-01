#pragma once

#include <liberay/math/vec.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <unordered_set>

namespace mini {

class Selection {
 public:
  void remove(Scene& scene, const SceneObjectHandle& handle) {
    objs_.erase(objs_.find(handle));
    update_centroid(scene);
  }

  void add(Scene& scene, const SceneObjectHandle& handle) {
    objs_.insert(handle);
    update_centroid(scene);
  }

  void clear() { objs_.clear(); }

  bool is_multi_selection() const { return objs_.size() > 1; }
  bool is_single_selection() const { return objs_.size() == 1; }
  bool is_empty() const { return objs_.empty(); }
  bool contains(const SceneObjectHandle& handle) const { return objs_.contains(handle); }

  SceneObjectHandle first() { return *objs_.begin(); }

  const eray::math::Vec3f& centroid() const { return centroid_; }

  auto begin() { return objs_.begin(); }
  auto begin() const { return objs_.begin(); }
  auto end() { return objs_.end(); }
  auto end() const { return objs_.end(); }

 private:
  void update_centroid(Scene& scene);

 private:
  eray::math::Vec3f centroid_;
  std::unordered_set<SceneObjectHandle> objs_;
};

}  // namespace mini
