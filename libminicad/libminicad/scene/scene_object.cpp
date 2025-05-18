#include <algorithm>
#include <expected>
#include <generator>
#include <liberay/math/vec.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <ranges>
#include <variant>

namespace mini {

namespace util = eray::util;
namespace math = eray::math;

SceneObject::SceneObject(SceneObjectHandle handle, Scene& scene)
    : ObjectBase<SceneObject, SceneObjectVariant>(handle, scene) {
  scene.renderer().push_object_rs_cmd(SceneObjectRSCommand(handle, SceneObjectRSCommand::Internal::AddObject{}));
}

bool SceneObject::can_be_deleted() { return patch_surfaces_.empty(); }

void SceneObject::on_delete() {
  if (!can_be_deleted()) {
    eray::util::Logger::err("Requested deletion of a scene object, however it cannot be deleted.");
    return;
  }

  scene().renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::Internal::DeleteObject{}));

  if (has_type<Point>()) {
    // Only points might be a part of the curve

    for (const auto& c_h : curves_) {
      if (auto pl = scene().arena<Curve>().get_obj(c_h)) {
        if (!pl.value()->points_.remove(*this)) {
          pl.value()->update();
          std::visit(
              eray::util::match{[&](auto& obj) { obj.on_point_remove(*pl.value(), *this, get_variant<Point>()); }},
              pl.value()->object);
        }
      }
    }
  }
}

void SceneObject::update() {
  scene().renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::UpdateObjectMembers{}));

  // Only points might be a part of the point lists
  if (has_type<Point>()) {
    for (const auto& c_h : this->curves_) {
      if (auto pl = scene().arena<Curve>().get_obj(c_h)) {
        scene().renderer().push_object_rs_cmd(CurveRSCommand(c_h, CurveRSCommand::Internal::UpdateControlPoints{}));
        std::visit(eray::util::match{[&](auto& obj) { obj.on_point_update(*pl.value(), *this, get_variant<Point>()); }},
                   pl.value()->object);
      }
    }
    for (const auto& ps_h : this->patch_surfaces_) {
      if (auto pl = scene().arena<PatchSurface>().get_obj(ps_h)) {
        scene().renderer().push_object_rs_cmd(
            PatchSurfaceRSCommand(ps_h, PatchSurfaceRSCommand::Internal::UpdateControlPoints{}));
      }
    }
  }
}

Curve::Curve(const CurveHandle& handle, Scene& scene) : ObjectBase<Curve, CurveVariant>(handle, scene) {
  scene.renderer().push_object_rs_cmd(CurveRSCommand(handle, CurveRSCommand::Internal::AddObject{}));
}

void Curve::on_delete() {
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::DeleteObject{}));
  for (auto& p : points_.point_objects()) {
    p.curves_.erase(handle_);
  }
}

void Curve::update() {
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));
}

std::expected<void, Curve::SceneObjectError> Curve::add(const SceneObjectHandle& handle) {
  if (points_.contains(handle)) {
    return {};
  }

  if (auto o = scene().arena<SceneObject>().get_obj(handle)) {
    auto& obj = *o.value();

    if (std::holds_alternative<Point>(obj.object)) {
      auto result = points_.add(obj);
      if (!result) {
        return std::unexpected(static_cast<SceneObjectError>(result.error()));
      }

      obj.curves_.insert(handle_);

      if (std::holds_alternative<BSplineCurve>(this->object)) {
        std::get<BSplineCurve>(this->object).reset_bernstein_points(*this);
        scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::UpdateHelperPoints{}));
      }
      std::visit(eray::util::match{[&](auto& o) { o.on_point_remove(*this, obj, obj.get_variant<Point>()); }},
                 this->object);
      scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

      return {};
    }
    auto obj_type_name = std::visit(util::match{[&](auto& p) { return p.type_name(); }}, obj.object);
    util::Logger::warn(
        "Detected an attempt to add a scene object that holds type \"{}\" to point list. Only objects of type \"{}\" "
        "are accepted.",
        obj_type_name, Point::type_name());

    return std::unexpected(SceneObjectError::NotAPoint);
  }

  return std::unexpected(SceneObjectError::InvalidHandle);
}

std::expected<void, Curve::SceneObjectError> Curve::remove(const SceneObjectHandle& handle) {
  if (!points_.contains(handle)) {
    return std::unexpected(SceneObjectError::NotFound);
  }

  if (auto o = scene().arena<SceneObject>().get_obj(handle)) {
    auto& obj = *o.value();
    if (obj.has_type<Point>()) {
      auto result = points_.remove(obj);
      if (!result) {
        return std::unexpected(static_cast<SceneObjectError>(result.error()));
      }

      auto obj_it = obj.curves_.find(handle_);
      if (obj_it != obj.curves_.end()) {
        obj.curves_.erase(obj_it);
      }
      std::visit(eray::util::match{[&](auto& o) { o.on_point_remove(*this, obj, obj.get_variant<Point>()); }},
                 this->object);
      scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

      return {};
    }

    return std::unexpected(SceneObjectError::NotAPoint);
  }

  return std::unexpected(SceneObjectError::InvalidHandle);
}

std::expected<void, Curve::SceneObjectError> Curve::move_before(const SceneObjectHandle& dest,
                                                                const SceneObjectHandle& obj) {
  if (!scene().arena<SceneObject>().exists(dest) || !scene().arena<SceneObject>().exists(obj)) {
    return std::unexpected(SceneObjectError::InvalidHandle);
  }

  auto result = points_.move_before(dest, obj);
  if (!result) {
    return std::unexpected(static_cast<SceneObjectError>(result.error()));
  }

  std::visit(eray::util::match{[&](auto& o) { o.on_curve_reorder(*this); }}, this->object);
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

  return {};
}

std::expected<void, Curve::SceneObjectError> Curve::move_after(const SceneObjectHandle& dest,
                                                               const SceneObjectHandle& obj) {
  if (!scene().arena<SceneObject>().exists(dest) || !scene().arena<SceneObject>().exists(obj)) {
    return std::unexpected(SceneObjectError::InvalidHandle);
  }

  auto result = points_.move_before(dest, obj);
  if (!result) {
    return std::unexpected(static_cast<SceneObjectError>(result.error()));
  }

  std::visit(eray::util::match{[&](auto& o) { o.on_curve_reorder(*this); }}, this->object);
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

  return {};
}

std::generator<eray::math::Vec3f> Curve::bezier3_points() const {
  return std::visit(eray::util::match{[&](const auto& o) { return o.bezier3_points(*this); }}, this->object);
}

size_t Curve::bezier3_points_count() const {
  return std::visit(eray::util::match{[&](const auto& o) { return o.bezier3_points_count(*this); }}, this->object);
}

std::generator<eray::math::Vec3f> Curve::polyline_points() const {
  auto lines = points() | std::views::adjacent<2>;
  for (const auto& [p0, p1] : lines) {
    co_yield p0;
    co_yield p1;
  }
}

size_t Curve::polyline_points_count() const { return 2 * (points_.size() - 1); }

std::generator<eray::math::Vec3f> Polyline::bezier3_points(ref<const Curve> base) const {
  auto&& points = base.get().points();
  auto lines    = points | std::views::adjacent<2>;
  for (const auto& [p0, p1] : lines) {
    co_yield p0;
    co_yield p1;
    co_yield p1;
    co_yield p1;
  }
}

size_t Polyline::bezier3_points_count(ref<const Curve> base) const { return (base.get().points().size() - 1) * 4; }

std::generator<eray::math::Vec3f> MultisegmentBezierCurve::bezier3_points(ref<const Curve> base) const {
  auto&& points = base.get().points();
  auto count    = points.size();
  if (points.empty()) {
    co_return;
  }

  auto bezier_range = points | std::views::slide(4) | std::views::stride(3) | std::views::join;
  for (const auto& p : bezier_range) {
    co_yield p;
  }

  // Fill up the rest points
  auto reminder   = (count - 1) % 3;
  auto last_point = *std::ranges::prev(points.end());
  if (reminder > 0) {
    auto last_points = points | std::views::drop(count - (reminder + 1));
    for (const auto& p : last_points) {
      co_yield p;
    }
    for (auto i = 0U; i < 3 - reminder; ++i) {
      co_yield last_point;
    }
  }
}

size_t MultisegmentBezierCurve::bezier3_points_count(ref<const Curve> base) const {
  auto cp_count = base.get().point_objects().size();
  if (cp_count == 0) {
    return 0;
  }

  if ((cp_count - 1) % 3 != 0) {
    return cp_count / 3 * 4 + 4;
  }
  return cp_count / 3 * 4;
}

void BSplineCurve::reset_bernstein_points(const Curve& base) {
  auto de_boor_points_count = base.point_objects().size();
  if (de_boor_points_count < 4) {
    bezier_points_.clear();
    return;
  }

  auto de_boor_points = base.point_objects() |
                        std::views::transform([](const SceneObject& s) { return s.transform.pos(); }) |
                        std::views::adjacent<3>;

  bezier_points_.clear();
  for (auto i = 0U; const auto& [p0, p1, p2] : de_boor_points) {
    bezier_points_.push_back((p0 + 4 * p1 + p2) / 6.0);
    if (de_boor_points_count - i > 3) {
      bezier_points_.push_back((4 * p1 + 2 * p2) / 6.0);
      bezier_points_.push_back((2 * p1 + 4 * p2) / 6.0);
    }
    ++i;
  }
}

void BSplineCurve::update_bernstein_segment(const Curve& base, int cp_idx) {
  auto de_boor_points = base.point_objects();

  if (cp_idx + 3 >= static_cast<int>(de_boor_points.size()) || cp_idx < 0) {
    return;
  }

  auto p0 = de_boor_points[cp_idx].transform.pos();
  auto p1 = de_boor_points[cp_idx + 1].transform.pos();
  auto p2 = de_boor_points[cp_idx + 2].transform.pos();
  auto p3 = de_boor_points[cp_idx + 3].transform.pos();

  auto cp_idx_u                    = static_cast<size_t>(cp_idx);
  bezier_points_[3 * cp_idx_u]     = (p0 + 4 * p1 + p2) / 6.0;
  bezier_points_[3 * cp_idx_u + 1] = (4 * p1 + 2 * p2) / 6.0;
  bezier_points_[3 * cp_idx_u + 2] = (2 * p1 + 4 * p2) / 6.0;
  bezier_points_[3 * cp_idx_u + 3] = (p1 + 4 * p2 + p3) / 6.0;
}

void BSplineCurve::update_bernstein_points(Curve& base, const SceneObjectHandle& handle) {
  if (auto o = base.point_idx(handle)) {
    auto cp_idx = static_cast<int>(o.value());
    update_bernstein_segment(base, cp_idx - 2);
    update_bernstein_segment(base, cp_idx - 1);
    update_bernstein_segment(base, cp_idx);
  }
}

void BSplineCurve::set_bernstein_point(Curve& base, size_t idx, const eray::math::Vec3f& point) {
  if (bezier_points_.size() <= idx) {
    return;
  }

  size_t cp_idx = idx / 3;
  if (idx == bezier_points_.size() - 1) {
    --cp_idx;
  }

  bezier_points_[idx] = point;

  auto de_boor_control_points_windows = base.point_objects() | std::views::slide(4);
  auto de_boor_points_to_update       = *(de_boor_control_points_windows.begin() + static_cast<int>(cp_idx));
  auto& p0                            = de_boor_points_to_update[0];
  auto& p1                            = de_boor_points_to_update[1];
  auto& p2                            = de_boor_points_to_update[2];
  auto& p3                            = de_boor_points_to_update[3];

  const auto& bp0 = bezier_points_[3 * cp_idx];
  const auto& bp1 = bezier_points_[3 * cp_idx + 1];
  const auto& bp2 = bezier_points_[3 * cp_idx + 2];
  const auto& bp3 = bezier_points_[3 * cp_idx + 3];

  p0.transform.set_local_pos(6 * bp0 - 7 * bp1 + 2 * bp2);
  p1.transform.set_local_pos(2 * bp1 - bp2);
  p2.transform.set_local_pos(-bp1 + 2 * bp2);
  p3.transform.set_local_pos(2 * bp1 - 7 * bp2 + 6 * bp3);

  // TODO(migoox): update points list that depend on these points
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

void BSplineCurve::on_point_update(Curve& base, const SceneObject& point, const Point&) {
  update_bernstein_points(base, point.handle());
  base.scene().renderer().push_object_rs_cmd(CurveRSCommand(base.handle(), CurveRSCommand::UpdateHelperPoints{}));
}

void BSplineCurve::on_point_add(Curve& base, const SceneObject&, const Point&) {
  reset_bernstein_points(base);
  base.scene().renderer().push_object_rs_cmd(CurveRSCommand(base.handle(), CurveRSCommand::UpdateHelperPoints{}));
}

void BSplineCurve::on_point_remove(Curve& base, const SceneObject&, const Point&) {
  reset_bernstein_points(base);
  base.scene().renderer().push_object_rs_cmd(CurveRSCommand(base.handle(), CurveRSCommand::UpdateHelperPoints{}));
}

void BSplineCurve::on_curve_reorder(Curve& base) {
  reset_bernstein_points(base);
  base.scene().renderer().push_object_rs_cmd(CurveRSCommand(base.handle(), CurveRSCommand::UpdateHelperPoints{}));
}

std::generator<eray::math::Vec3f> BSplineCurve::bezier3_points(ref<const Curve> /*base*/) const {
  for (auto i = 0U; const auto& b : bezier_points_) {
    co_yield b;
    if (i % 3 == 0 && i != 0) {
      co_yield b;
    }
    ++i;
  }
}

std::generator<eray::math::Vec3f> BSplineCurve::unique_bezier3_points(ref<const Curve> /*base*/) const {
  for (const auto& b : bezier_points_) {
    co_yield b;
  }
}

size_t BSplineCurve::bezier3_points_count(ref<const Curve> /*base*/) const { return bezier_points_.size() / 3 * 4; }

size_t BSplineCurve::unique_bezier3_points_count(ref<const Curve> /*base*/) const { return bezier_points_.size(); }

void NaturalSplineCurve::reset_segments(Curve& base) {
  auto all_points_count = base.point_objects().size();

  if (all_points_count == 0) {
    segments_.clear();
    return;
  }

  unique_points_.clear();
  unique_points_.reserve(all_points_count);
  for (const auto& [p0, p1] : base.points() | std::views::adjacent<2>) {
    if (math::distance(p0, p1) > NaturalSplineCurve::Segment::kLengthEpsilon) {
      unique_points_.push_back(p0);
    }
  }
  unique_points_.push_back(base.points().back());

  auto n = unique_points_.size();

  // Indices
  //   - points: 0, 1, ..., n-1
  //   - segments: 0, 1, ..., n-2
  //   - matrix coefficients: 0, 1, ..., n-3

  segments_.clear();
  segments_.resize(n - 1, {});
  if (n == 2) {
    auto p0                   = unique_points_.front();
    auto p1                   = *(++unique_points_.begin());
    segments_[0].chord_length = math::distance(p0, p1);
    segments_[0].a            = p0;
    segments_[0].b            = (p1 - p0) / segments_[0].chord_length;
    segments_[0].c            = math::Vec3f::filled(0.F);
    segments_[0].d            = math::Vec3f::filled(0.F);
  }

  if (n < 3) {
    return;
  }

  // Find the lengths
  for (const auto& [i, ps] : unique_points_ | std::views::adjacent<2> | std::views::enumerate) {
    auto [p0, p1] = ps;
    auto idx      = static_cast<size_t>(i);

    segments_[idx].chord_length = math::distance(p0, p1);
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
  for (const auto& [i, ps] : unique_points_ | std::views::adjacent<3> | std::views::enumerate) {
    const auto& [p0, p1, p2] = ps;
    auto idx                 = static_cast<size_t>(i);

    auto nom         = (p2 - p1) / segments_[idx + 1].chord_length - (p1 - p0) / segments_[idx].chord_length;
    auto denom       = segments_[idx + 1].chord_length + segments_[idx].chord_length;
    segments_[idx].b = 3.F * nom / denom;
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
  auto points_without_start_end = unique_points_ | std::views::take(n - 1);  // skip the last point
  for (auto it = segments_.begin(); const auto& p : points_without_start_end) {
    it->a = p;
    ++it;
  }

  // Find the d power basis coefficients from the C^2 constraint (excluding n-2)
  for (auto i = 0U; i < n - 2; ++i) {
    segments_[i].d = (segments_[i + 1].c - segments_[i].c) / (3.F * segments_[i].chord_length);
  }

  // Find the b power basis coefficients from the C^0 constraint (excluding n-2)
  auto point_it = ++unique_points_.cbegin();  // skip the first point
  for (auto i = 0U; i < n - 2; ++i) {
    segments_[i].b = (*point_it - segments_[i].a) / segments_[i].chord_length -
                     segments_[i].c * segments_[i].chord_length -
                     segments_[i].d * segments_[i].chord_length * segments_[i].chord_length;
    ++point_it;
  }

  // Find the constrains for n-2
  segments_[n - 2].b = segments_[n - 3].b + 2.F * segments_[n - 3].c * segments_[n - 3].chord_length +
                       3.F * segments_[n - 3].d * segments_[n - 3].chord_length * segments_[n - 3].chord_length;

  auto last_point = *(--unique_points_.cend());
  auto l          = segments_[n - 2].chord_length;
  segments_[n - 2].d =
      (last_point - segments_[n - 2].a) / (l * l * l) - segments_[n - 2].b / (l * l) - segments_[n - 2].c / l;
}

void NaturalSplineCurve::update(Curve& base) { reset_segments(base); }

std::generator<eray::math::Vec3f> NaturalSplineCurve::bezier3_points(ref<const Curve> /*base*/) const {
  for (const auto& s : segments_) {
    co_yield s.a;
    co_yield s.a + 1.F / 3.F * s.b;
    co_yield s.a + 2.F / 3.F * s.b + 1.F / 3.F * s.c;
    co_yield s.a + s.b + s.c + s.d;
  }
}

size_t NaturalSplineCurve::bezier3_points_count(ref<const Curve> /*base*/) const { return segments_.size() * 4; }

PatchSurface::PatchSurface(const PatchSurfaceHandle& handle, Scene& scene)
    : ObjectBase<PatchSurface, PatchSurfaceVariant>(handle, scene) {
  scene.renderer().push_object_rs_cmd(PatchSurfaceRSCommand(handle, PatchSurfaceRSCommand::Internal::AddObject{}));
}

// void PatchSurface::make_plane(eray::math::Vec2u dim, eray::math::Vec2f patch_size) {
//   auto handles_to_delete = std::vector<SceneObjectHandle>();
//   for (auto col = 0U; col < dim_.x; ++col) {
//     for (auto row = 0U; row < dim_.y; ++row) {
//       if (col < dim.x && row < dim.y) {
//         continue;
//       }

//       // Save patch control points to delete
//       auto start_in_col = col == 0 ? 0U : 1U;
//       auto start_in_row = row == 0 ? 0U : 1U;
//       for (auto in_col = start_in_col; in_col < kPatchSize; ++in_col) {
//         for (auto in_row = start_in_row; in_row < kPatchSize; ++in_row) {
//           handles_to_delete.push_back(points_.unsafe_by_idx(find_idx(col, row, in_col, in_row, dim_.x)).handle());
//         }
//       }
//     }
//   }
//   dim_.x = std::min(dim_.x, dim.x);
//   dim_.y = std::min(dim_.y, dim.y);

//   points_.remove_many(handles_to_delete);
//   for (const auto& h : handles_to_delete) {
//     if (auto o = scene().arena<SceneObject>().get_obj(h)) {
//       auto& obj = *o.value();
//       auto it   = obj.patch_surfaces_.find(handle());
//       if (it == obj.patch_surfaces_.end()) {
//         eray::util::Logger::warn(
//             "Attempted to unregister a point list from a point, but the point did not reference that point list.");
//         continue;
//       }
//       obj.patch_surfaces_.erase(it);
//       if (obj.can_be_deleted()) {
//         scene().delete_obj(obj.handle());
//       }
//     }
//   }

//   size_t dx = dim.x - dim_.x;
//   size_t dy = dim.y - dim_.y;

//   size_t new_points_count =
//       dx * (kPatchSize - 1) * (dim_.y * (kPatchSize - 1) + 1) + dy * (kPatchSize - 1) * (dim.x * (kPatchSize - 1) +
//       1);

//   auto new_point_handles = scene().create_many_objs<SceneObject>(Point{}, new_points_count);
//   auto new_points = std::vector<SceneObjectHandle>(((kPatchSize - 1) * dim_.x + 1) * ((kPatchSize - 1) * dim_.y + 1),
//                                                    SceneObjectHandle(0, 0, 0));

//   auto np_it = new_point_handles->begin();
//   for (auto col = 0U; col < dim.x; ++col) {
//     for (auto row = 0U; row < dim.y; ++row) {
//       auto start_in_col = col == 0 ? 0U : 1U;
//       auto start_in_row = row == 0 ? 0U : 1U;
//       for (auto in_col = start_in_col; in_col < kPatchSize; ++in_col) {
//         for (auto in_row = start_in_row; in_row < kPatchSize; ++in_row) {
//           auto idx = find_idx(col, row, in_col, in_row, dim.x);
//           if (col < dim_.x && row < dim_.y) {
//             auto old_idx    = find_idx(col, row, in_col, in_row, dim_.x);
//             new_points[idx] = points_.unsafe_by_idx(old_idx).handle();
//           } else {
//             new_points[idx] = *np_it;
//             ++np_it;
//           }
//         }
//       }
//     }
//   }
//   points_.unsafe_set(scene(), new_points);
//   dim_ = dim;
// }

void PatchSurface::make_plane(eray::math::Vec2u dim, eray::math::Vec2f size) {
  dim.x = std::max(dim.x, 1U);
  dim.y = std::max(dim.y, 1U);

  for (const auto& h : points_.point_handles()) {
    if (auto o = scene().arena<SceneObject>().get_obj(h)) {
      auto& obj = *o.value();
      auto it   = obj.patch_surfaces_.find(handle());
      if (it == obj.patch_surfaces_.end()) {
        eray::util::Logger::warn(
            "Attempted to unregister a point list from a point, but the point did not reference that point list.");
        continue;
      }
      obj.patch_surfaces_.erase(it);
      if (obj.can_be_deleted()) {
        scene().delete_obj(obj.handle());
      }
    }
  }
  points_.clear();

  dim_                  = dim;
  auto new_points_count = (dim.x * (kPatchSize - 1) + 1) * (dim.y * (kPatchSize - 1) + 1);

  auto new_point_handles = scene().create_many_objs<SceneObject>(Point{}, new_points_count);
  if (!new_point_handles) {
    util::Logger::err("Could not create new point objects");
    return;
  }
  points_.unsafe_set(scene(), *new_point_handles);

  for (auto col = 0U; col < dim_.x; ++col) {
    for (auto row = 0U; row < dim_.y; ++row) {
      auto start_in_col = col == 0 ? 0U : 1U;
      auto start_in_row = row == 0 ? 0U : 1U;
      for (auto in_col = start_in_col; in_col < kPatchSize; ++in_col) {
        for (auto in_row = start_in_row; in_row < kPatchSize; ++in_row) {
          auto p = eray::math::Vec3f::filled(0.F);
          p.x    = size.x * static_cast<float>(col * (kPatchSize - 1) + in_col) /
                static_cast<float>(dim_.x * (kPatchSize - 1) + 1);
          p.z = size.y * static_cast<float>(row * (kPatchSize - 1) + in_row) /
                static_cast<float>(dim_.y * (kPatchSize - 1) + 1);

          auto& p_obj = points_.unsafe_by_idx(find_idx(col, row, in_col, in_row, dim_.x));
          p_obj.transform.set_local_pos(p);
          p_obj.update();
          p_obj.patch_surfaces_.emplace(handle());
        }
      }
    }
  }
}

std::generator<eray::math::Vec3f> PatchSurface::control_points() const {
  for (const auto& p : points()) {
    co_yield p;
  }
}

size_t PatchSurface::control_points_count() const { return points_.size(); }

std::generator<std::uint32_t> PatchSurface::grids_indices() const {
  for (auto col = 0U; col < (kPatchSize - 1) * dim_.x + 1; ++col) {
    for (auto row = 0U; row < (kPatchSize - 1) * dim_.y; ++row) {
      co_yield row*((kPatchSize - 1) * dim_.x + 1) + col;
      co_yield (row + 1) * ((kPatchSize - 1) * dim_.x + 1) + col;
    }
  }

  for (auto row = 0U; row < (kPatchSize - 1) * dim_.y + 1; ++row) {
    for (auto col = 0U; col < (kPatchSize - 1) * dim_.x; ++col) {
      co_yield row*((kPatchSize - 1) * dim_.y + 1) + col;
      co_yield row*((kPatchSize - 1) * dim_.y + 1) + col + 1;
    }
  }
}

size_t PatchSurface::grids_indices_count() const {
  return 2 * (((kPatchSize - 1) * dim_.y + 1) * (kPatchSize - 1) * dim_.x +
              ((kPatchSize - 1) * dim_.x + 1) * (kPatchSize - 1) * dim_.y);
}

std::generator<eray::math::Vec3f> PatchSurface::bezier3_points() const { return control_points(); }

size_t PatchSurface::bezier3_points_count() const { return points_.size(); }

std::generator<std::uint32_t> PatchSurface::bezier3_indices() const {
  for (auto row = 0U; row < dim_.y; ++row) {
    for (auto col = 0U; col < dim_.x; ++col) {
      for (auto in_row = 0U; in_row < kPatchSize; ++in_row) {
        for (auto in_col = 0U; in_col < kPatchSize; ++in_col) {
          co_yield static_cast<std::uint32_t>(find_idx(col, row, in_col, in_row, dim_.x));
        }
      }
    }
  }
}

size_t PatchSurface::bezier3_indices_count() const { return dim_.x * dim_.y * kPatchSize * kPatchSize; }

}  // namespace mini
