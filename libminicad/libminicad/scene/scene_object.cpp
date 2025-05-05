#include <algorithm>
#include <expected>
#include <liberay/math/vec.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <ranges>
#include <variant>

namespace mini {

namespace util = eray::util;
namespace math = eray::math;

SceneObject::~SceneObject() {
  scene_.renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::Internal::DeleteObject{}));

  for (const auto& pl_h : point_lists_) {
    if (auto pl = scene_.get_obj(pl_h)) {
      auto it = pl.value()->points_map_.find(handle_);
      if (it != pl.value()->points_map_.end()) {
        auto ind = it->second;
        pl.value()->mark_dirty();
        pl.value()->points_map_.erase(it);
        pl.value()->points_.erase(pl.value()->points_.begin() + static_cast<int>(ind));
        pl.value()->update_indices_from(ind);
        update_point_list(*pl.value());
      }
    }
  }
}

void SceneObject::mark_dirty() {
  scene_.renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::UpdateObjectMembers{}));

  // Only points might be a part of the point lists
  if (std::holds_alternative<Point>(object)) {
    for (const auto& pl_h : this->point_lists_) {
      if (auto pl = scene_.get_obj(pl_h)) {
        update_point_list(*pl.value());
      }
    }
  }
}

void SceneObject::update_point_list(PointListObject& obj) {
  // Only points might be a part of the point lists
  if (!std::holds_alternative<Point>(object)) {
    return;
  }

  std::visit(eray::util::match{//
                               [&](BSplineCurve& curve) {
                                 curve.reset_bernstein_points(obj);
                                 scene_.renderer().push_object_rs_cmd(PointListObjectRSCommand(
                                     obj.handle(), PointListObjectRSCommand::UpdateBernsteinControlPoints{}));
                               },  //
                               [&](NaturalSplineCurve& curve) {
                                 curve.reset_segments(obj);
                                 scene_.renderer().push_object_rs_cmd(PointListObjectRSCommand(
                                     obj.handle(), PointListObjectRSCommand::UpdateNaturalSplineSegments{}));
                               },  //
                               [](auto&) {}},
             obj.object);
}

PointListObject::~PointListObject() {
  scene_.renderer().push_object_rs_cmd(
      PointListObjectRSCommand(handle_, PointListObjectRSCommand::Internal::DeleteObject{}));
  for (auto p : points_) {
    p.get().point_lists_.erase(handle_);
  }
}

void PointListObject::mark_dirty() {
  scene_.renderer().push_object_rs_cmd(
      PointListObjectRSCommand(handle_, PointListObjectRSCommand::UpdateObjectMembers{}));
}

std::expected<void, PointListObject::SceneObjectError> PointListObject::add(const SceneObjectHandle& handle) {
  if (points_map_.contains(handle)) {
    return {};
  }

  if (!scene_.is_handle_valid(handle)) {
    return std::unexpected(SceneObjectError::HandleIsNotValid);
  }

  auto& obj = *scene_.scene_objects_.at(handle.obj_id)->first;
  if (std::holds_alternative<Point>(obj.object)) {
    auto idx = points_.size();
    points_map_.insert({handle, idx});
    points_.emplace_back(obj);

    obj.point_lists_.insert(handle_);

    if (std::holds_alternative<BSplineCurve>(this->object)) {
      std::get<BSplineCurve>(this->object).reset_bernstein_points(*this);
      scene_.renderer().push_object_rs_cmd(
          PointListObjectRSCommand(handle_, PointListObjectRSCommand::UpdateBernsteinControlPoints{}));
    }

    mark_dirty();

    return {};
  }

  auto obj_type_name = std::visit(util::match{[&](auto& p) { return p.type_name(); }}, obj.object);
  util::Logger::warn(
      "Detected an attempt to add a scene object that holds type \"{}\" to point list. Only objects of type \"{}\" "
      "are accepted.",
      obj_type_name, Point::type_name());

  return std::unexpected(SceneObjectError::NotAPoint);
}

std::expected<void, PointListObject::SceneObjectError> PointListObject::remove(const SceneObjectHandle& handle) {
  if (!scene_.is_handle_valid(handle)) {
    return std::unexpected(SceneObjectError::HandleIsNotValid);
  }

  if (!points_map_.contains(handle)) {
    return std::unexpected(SceneObjectError::NotFound);
  }

  auto& obj = *scene_.scene_objects_.at(handle.obj_id)->first;
  if (std::holds_alternative<Point>(obj.object)) {
    auto it = points_map_.find(handle);
    if (it != points_map_.end()) {
      auto idx = it->second;

      points_map_.erase(it);
      points_.erase(points_.begin() + static_cast<int>(idx));
    }

    auto obj_it = obj.point_lists_.find(handle_);
    if (obj_it != obj.point_lists_.end()) {
      obj.point_lists_.erase(obj_it);
    }
    mark_dirty();

    return {};
  }

  return std::unexpected(SceneObjectError::NotAPoint);
}

std::expected<void, PointListObject::SceneObjectError> PointListObject::move_before(const SceneObjectHandle& dest,
                                                                                    const SceneObjectHandle& obj) {
  if (!scene_.is_handle_valid(dest) || !scene_.is_handle_valid(obj)) {
    return std::unexpected(SceneObjectError::HandleIsNotValid);
  }

  if (!points_map_.contains(dest) || !points_map_.contains(obj)) {
    return std::unexpected(SceneObjectError::NotFound);
  }

  size_t dest_idx = points_map_[dest];
  size_t obj_idx  = points_map_[obj];

  if (obj_idx == dest_idx) {
    return {};
  }
  SceneObject& obj_ref = points_[obj_idx].get();
  points_.erase(points_.begin() + static_cast<int>(obj_idx));
  if (obj_idx < dest_idx && dest_idx > 0) {
    dest_idx--;
  }
  points_.insert(points_.begin() + static_cast<int>(dest_idx), std::ref(obj_ref));
  update_indices_from(std::min(dest_idx, obj_idx));

  return {};
}

std::expected<void, PointListObject::SceneObjectError> PointListObject::move_after(const SceneObjectHandle& dest,
                                                                                   const SceneObjectHandle& obj) {
  if (!scene_.is_handle_valid(dest) || !scene_.is_handle_valid(obj)) {
    return std::unexpected(SceneObjectError::HandleIsNotValid);
  }

  if (!points_map_.contains(dest) || !points_map_.contains(obj)) {
    return std::unexpected(SceneObjectError::NotFound);
  }

  size_t dest_idx = points_map_[dest];
  size_t obj_idx  = points_map_[obj];

  if (obj_idx == dest_idx + 1) {
    return {};
  }

  SceneObject& obj_ref = points_[obj_idx].get();
  points_.erase(points_.begin() + static_cast<int>(obj_idx));
  size_t insert_pos = dest_idx + 1;
  if (obj_idx > dest_idx && insert_pos > 0) {
    insert_pos--;
  }
  insert_pos = std::min(insert_pos, points_.size());
  points_.insert(points_.begin() + static_cast<int>(insert_pos), std::ref(obj_ref));
  update_indices_from(std::min(insert_pos, obj_idx));

  return {};
}

void PointListObject::update_indices_from(size_t start_idx) {
  for (size_t i = start_idx; i < points_.size(); ++i) {
    points_map_[points_[i].get().handle()] = i;
  }
}

void BSplineCurve::reset_bernstein_points(const PointListObject& base) {
  auto de_boor_points_count = base.point_objects().size();
  if (de_boor_points_count < 4) {
    bernstein_points_.clear();
    return;
  }

  auto de_boor_points = base.point_objects() |
                        std::views::transform([](const SceneObject& s) { return s.transform.pos(); }) |
                        std::views::adjacent<3>;

  bernstein_points_.clear();
  for (auto i = 0U; const auto& [p0, p1, p2] : de_boor_points) {
    bernstein_points_.push_back((p0 + 4 * p1 + p2) / 6.0);
    if (de_boor_points_count - i > 3) {
      bernstein_points_.push_back((4 * p1 + 2 * p2) / 6.0);
      bernstein_points_.push_back((2 * p1 + 4 * p2) / 6.0);
    }
    ++i;
  }
}

void BSplineCurve::update_bernstein_segment(const PointListObject& base, int cp_idx) {
  auto de_boor_points = base.point_objects();

  if (cp_idx + 3 >= static_cast<int>(de_boor_points.size()) || cp_idx < 0) {
    return;
  }

  auto p0 = de_boor_points[cp_idx].transform.pos();
  auto p1 = de_boor_points[cp_idx + 1].transform.pos();
  auto p2 = de_boor_points[cp_idx + 2].transform.pos();
  auto p3 = de_boor_points[cp_idx + 3].transform.pos();

  auto cp_idx_u                       = static_cast<size_t>(cp_idx);
  bernstein_points_[3 * cp_idx_u]     = (p0 + 4 * p1 + p2) / 6.0;
  bernstein_points_[3 * cp_idx_u + 1] = (4 * p1 + 2 * p2) / 6.0;
  bernstein_points_[3 * cp_idx_u + 2] = (2 * p1 + 4 * p2) / 6.0;
  bernstein_points_[3 * cp_idx_u + 3] = (p1 + 4 * p2 + p3) / 6.0;
}

void BSplineCurve::update_bernstein_points(PointListObject& base, const SceneObjectHandle& handle) {
  if (auto o = base.point_idx(handle)) {
    auto cp_idx = static_cast<int>(o.value());
    update_bernstein_segment(base, cp_idx - 1);
    update_bernstein_segment(base, cp_idx);
    update_bernstein_segment(base, cp_idx + 1);
  }
}

void BSplineCurve::set_bernstein_point(PointListObject& base, size_t idx, const eray::math::Vec3f& point) {
  if (bernstein_points_.size() <= idx) {
    return;
  }

  size_t cp_idx = idx / 3;
  if (idx == bernstein_points_.size() - 1) {
    --cp_idx;
  }

  bernstein_points_[idx] = point;

  auto de_boor_control_points_windows = base.point_objects() | std::views::slide(4);
  auto de_boor_points_to_update       = *(de_boor_control_points_windows.begin() + static_cast<int>(cp_idx));
  auto& p0                            = de_boor_points_to_update[0];
  auto& p1                            = de_boor_points_to_update[1];
  auto& p2                            = de_boor_points_to_update[2];
  auto& p3                            = de_boor_points_to_update[3];

  const auto& bp0 = bernstein_points_[3 * cp_idx];
  const auto& bp1 = bernstein_points_[3 * cp_idx + 1];
  const auto& bp2 = bernstein_points_[3 * cp_idx + 2];
  const auto& bp3 = bernstein_points_[3 * cp_idx + 3];

  p0.transform.set_local_pos(6 * bp0 - 7 * bp1 + 2 * bp2);
  p1.transform.set_local_pos(2 * bp1 - bp2);
  p2.transform.set_local_pos(-bp1 + 2 * bp2);
  p3.transform.set_local_pos(2 * bp1 - 7 * bp2 + 6 * bp3);

  base.scene().renderer().push_object_rs_cmd(
      SceneObjectRSCommand(p0.handle(), SceneObjectRSCommand::UpdateObjectMembers{}));
  base.scene().renderer().push_object_rs_cmd(
      SceneObjectRSCommand(p1.handle(), SceneObjectRSCommand::UpdateObjectMembers{}));
  base.scene().renderer().push_object_rs_cmd(
      SceneObjectRSCommand(p2.handle(), SceneObjectRSCommand::UpdateObjectMembers{}));
  base.scene().renderer().push_object_rs_cmd(
      SceneObjectRSCommand(p3.handle(), SceneObjectRSCommand::UpdateObjectMembers{}));

  auto cp_idx_int = static_cast<int>(cp_idx);
  update_bernstein_segment(base, cp_idx_int - 2);
  update_bernstein_segment(base, cp_idx_int - 1);
  update_bernstein_segment(base, cp_idx_int + 1);
  update_bernstein_segment(base, cp_idx_int + 2);
}

void NaturalSplineCurve::update_point(PointListObject& /*base*/, const SceneObjectHandle& /*handle*/) {
  util::panic("Not implemented yet");
}

void NaturalSplineCurve::reset_segments(PointListObject& base) {
  auto all_points_count = base.point_objects().size();

  if (all_points_count == 0) {
    segments_.clear();
    return;
  }

  points_.reserve(all_points_count);
  points_.clear();
  for (const auto& [p0, p1] : base.points() | std::views::adjacent<2>) {
    if (math::distance(p0, p1) > NaturalSplineCurve::Segment::kLengthEpsilon) {
      points_.push_back(p0);
    }
  }
  points_.push_back(*(--base.points().cend()));

  auto n = points_.size();

  // Indices
  //   - points: 0, 1, ..., n-1
  //   - segments: 0, 1, ..., n-2
  //   - matrix coefficients: 0, 1, ..., n-3

  segments_.clear();
  segments_.resize(n - 1, {});
  if (n == 2) {
    auto p0                   = *points_.begin();
    auto p1                   = *(++points_.begin());
    segments_[0].chord_length = math::distance(p0, p1);
    segments_[0].a            = p0;
    segments_[0].b            = p1 / segments_[0].chord_length;
    segments_[0].c            = math::Vec3f::filled(0.F);
    segments_[0].d            = math::Vec3f::filled(0.F);
  }

  if (n < 3) {
    return;
  }

  // Find the lengths
  for (auto it = segments_.begin(); const auto& [p0, p1] : points_ | std::views::adjacent<2>) {
    it->chord_length = math::length(p1 - p0);
    ++it;
  }

  // Find the tridiagonal matrix coefficients, using the Segment struct to avoid additional memory allocation:
  //  - The diagonal itself has all cell values equal 2
  //  - a.x denotes the lower diagonal coefficients
  //  - a.y denotes the upper diagonal coefficients
  for (auto i = 0U; i < n - 2; ++i) {
    float denom      = segments_[i].chord_length + segments_[i + 1].chord_length;
    segments_[i].a.x = segments_[i].chord_length / denom;
    segments_[i].a.y = segments_[i + 1].chord_length / denom;
  }
  segments_[0].a.x     = 0;  // not used, we assume that the c_0=c_{n-1}=0
  segments_[n - 3].a.y = 0;

  // b denotes the right hand 3-dimensional coefficient vector
  for (auto i = 0U; const auto& [p0, p1, p2] : points_ | std::views::adjacent<3>) {
    auto nom       = (p2 - p1) / segments_[i + 1].chord_length - (p1 - p0) / segments_[i].chord_length;
    auto denom     = segments_[i + 1].chord_length + segments_[i].chord_length;
    segments_[i].b = 3.F * nom / denom;
    ++i;
  }

  // Solving the tridiagonal matrix. Based on: https://en.wikipedia.org/wiki/Tridiagonal_matrix_algorithm
  //
  // 1. Find the new coefficients
  //
  // a.z denotes the new upper diagonal coefficients
  segments_[0].a.z = segments_[0].a.y / 2.F;
  for (auto i = 1U; i < n - 2; ++i) {
    segments_[i].a.z = segments_[i].a.y / (2.F - segments_[i].a.x * segments_[i - 1].a.z);
  }
  //
  // d denotes the new right hand 3-dimensional coefficients vector
  //
  segments_[0].d = segments_[0].b / 2.F;
  for (auto i = 1U; i < n - 2; ++i) {
    auto nom       = segments_[i].b - segments_[i].a.x * segments_[i - 1].d;
    auto denom     = 2.F - segments_[i].a.x * segments_[i - 1].a.z;
    segments_[i].d = nom / denom;
  }
  //
  // 2. Find the solution -- c power basis coefficients
  //
  segments_[0].c     = eray::math::Vec3f::filled(0.F);  // from assumption that c_0=c_{n-1}=0
  segments_[n - 2].c = segments_[n - 3].d;
  for (auto i = static_cast<int>(n) - 3; i >= 1; --i) {
    auto idx         = static_cast<size_t>(i);
    segments_[idx].c = segments_[idx - 1].d - segments_[idx - 1].a.z * segments_[idx + 1].c;
  }

  // Find the a power basis coefficients from the fact that a_i = P_i, (interpolation constraint)
  auto points_without_start_end = points_ | std::views::take(n - 1);  // skip the last point
  for (auto it = segments_.begin(); const auto& p : points_without_start_end) {
    it->a = p;
    ++it;
  }

  // Find the d power basis coefficients from the C^2 constraint (excluding n-2)
  for (auto i = 0U; i < n - 2; ++i) {
    segments_[i].d = (segments_[i + 1].c - segments_[i].c) / (3.F * segments_[i].chord_length);
  }

  // Find the b power basis coefficients from the C^0 constraint (excluding n-2)
  auto point_it = ++points_.cbegin();  // skip the first point
  for (auto i = 0U; i < n - 2; ++i) {
    segments_[i].b = (*point_it - segments_[i].a) / segments_[i].chord_length -
                     segments_[i].c * segments_[i].chord_length -
                     segments_[i].d * segments_[i].chord_length * segments_[i].chord_length;
    ++point_it;
  }

  // Find the constrains for n-2
  segments_[n - 2].b = segments_[n - 3].b + 2.F * segments_[n - 3].c * segments_[n - 3].chord_length +
                       3.F * segments_[n - 3].d * segments_[n - 3].chord_length * segments_[n - 3].chord_length;

  auto last_point = *(--points_.cend());
  auto l          = segments_[n - 2].chord_length;
  segments_[n - 2].d =
      (last_point - segments_[n - 2].a) / (l * l * l) - segments_[n - 2].b / (l * l) - segments_[n - 2].c / l;
}

}  // namespace mini
