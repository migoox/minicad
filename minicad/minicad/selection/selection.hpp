#pragma once

#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <unordered_set>

namespace mini {

class Selection {
 public:
  void remove(Scene& scene, const SceneObjectHandle& handle);
  void add(Scene& scene, const SceneObjectHandle& handle);
  void clear(Scene& scene);

  bool is_multi_selection() const { return objs_.size() > 1; }
  bool is_single_selection() const { return objs_.size() == 1; }
  bool is_empty() const { return objs_.empty(); }
  bool contains(const SceneObjectHandle& handle) const { return objs_.contains(handle); }

  auto centroid() const { return transform.pos(); }

  SceneObjectHandle first() { return *objs_.begin(); }

  void mark_transform_dirty(Scene& scene);

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

  std::unordered_set<SceneObjectHandle> objs_;
};

}  // namespace mini
