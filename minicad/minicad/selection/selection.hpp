#pragma once

#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/util/iterator.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <minicad/cursor/cursor.hpp>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <variant>

namespace mini {

class NonTransformableSelection {
 public:
  void remove(const NonTransformableObjectHandle& handle) {
    auto f = objs_.find(handle);
    if (f != objs_.end()) {
      objs_.erase(f);
    }
  }
  void add(const NonTransformableObjectHandle& handle) { objs_.insert(handle); }
  void clear() { objs_.clear(); }

  bool contains(const NonTransformableObjectHandle& handle) const { return objs_.contains(handle); }
  bool is_multi_selection() const { return objs_.size() > 1; }
  bool is_single_selection() const { return objs_.size() == 1; }
  bool is_empty() const { return objs_.empty(); }

  std::optional<NonTransformableObjectHandle> single() {
    if (is_single_selection()) {
      return *objs_.begin();
    }
    return std::nullopt;
  }

  const NonTransformableObjectHandle& first() { return *objs_.begin(); }

  auto begin() { return objs_.begin(); }
  auto begin() const { return objs_.begin(); }
  auto end() { return objs_.end(); }
  auto end() const { return objs_.end(); }

 private:
  std::unordered_set<NonTransformableObjectHandle> objs_;
};

class TransformableSelection {
 public:
  void remove(Scene& scene, const TransformableObjectHandle& handle);
  void add(Scene& scene, const TransformableObjectHandle& handle);
  void clear(Scene& scene);

  template <eray::util::Iterator<TransformableObjectHandle> It>
  void add_many(Scene& scene, It begin, It end) {
    detach_all(scene);
    objs_.insert(begin, end);
    update_centroid(scene);
    update_is_points_only_selection();
  }

  template <eray::util::Iterator<TransformableObjectHandle> It>
  void remove_many(Scene& scene, It begin, It end) {
    detach_all(scene);
    for (const auto& handle : std::ranges::subrange(begin, end)) {
      auto f = objs_.find(handle);
      if (f != objs_.end()) {
        objs_.erase(f);
      }
    }
    update_centroid(scene);
    update_is_points_only_selection();
  }

  bool is_multi_selection() const { return objs_.size() > 1; }
  bool is_single_selection() const { return objs_.size() == 1; }
  bool is_empty() const { return objs_.empty(); }

  bool contains(const TransformableObjectHandle& handle) const { return objs_.contains(handle); }

  auto centroid() const { return centroid_; }

  const TransformableObjectHandle& first() { return *objs_.begin(); }
  std::optional<TransformableObjectHandle> single() {
    if (is_single_selection()) {
      return *objs_.begin();
    }
    return std::nullopt;
  }
  std::optional<PointObjectHandle> single_point() {
    if (is_single_selection() && std::holds_alternative<PointObjectHandle>(*objs_.begin())) {
      return std::get<PointObjectHandle>(*objs_.begin());
    }
    return std::nullopt;
  }

  void update_transforms(Scene& scene, Cursor& cursor);

  void set_custom_origin(Scene& scene, eray::math::Vec3f vec);
  void use_custom_origin(Scene& scene, bool use_custom_origin);
  bool is_using_custom_origin() const { return use_custom_origin_; }

  bool is_points_only() const { return points_only_; }

  auto points() const {
    auto is_point = [](const TransformableObjectHandle& h) { return std::holds_alternative<PointObjectHandle>(h); };
    auto to_point = [](const TransformableObjectHandle& h) { return std::get<PointObjectHandle>(h); };
    return objs_ | std::views::filter(is_point) | std::views::transform(to_point);
  }

  auto begin() { return objs_.begin(); }
  auto begin() const { return objs_.begin(); }
  auto end() { return objs_.end(); }
  auto end() const { return objs_.end(); }

 public:
  eray::math::Transform3f transform;

 private:
  void detach_all(Scene& scene);
  void update_centroid(Scene& scene);
  void update_is_points_only_selection();

 private:
  bool transform_dirty_;
  bool use_custom_origin_;

  bool points_only_;

  eray::math::Vec3f centroid_;
  eray::math::Vec3f custom_origin_;

  // NOTE: The hashes of  handles of different types are typically non colliding because of the timestamp.
  std::unordered_set<TransformableObjectHandle> objs_;
};

struct HelperPoint {
  CurveHandle parent;
  size_t helper_point;
};

class HelperPointSelection {
 public:
  bool is_selected() { return selection_.has_value(); }
  const std::optional<HelperPoint>& selection() { return selection_; }
  void set_selection(HelperPoint&& selection) { selection_ = std::move(selection); }
  void clear() { selection_ = std::nullopt; }

  void set_point(Scene& scene, eray::math::Vec3f new_pos);
  std::optional<eray::math::Vec3f> pos(Scene& scene);

 private:
  std::optional<HelperPoint> selection_;
};

}  // namespace mini
