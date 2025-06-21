#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <libminicad/math/bezier3.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/curve.hpp>
#include <libminicad/scene/scene.hpp>
#include <limits>

namespace mini {

namespace math = eray::math;
namespace util = eray::util;

Curve::Curve(const CurveHandle& handle, Scene& scene)
    : ObjectBase<Curve, CurveVariant>(handle, scene), bezier_dirty_(true) {
  scene.renderer().push_object_rs_cmd(CurveRSCommand(handle, CurveRSCommand::Internal::AddObject{}));
}

void Curve::on_delete() {
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::DeleteObject{}));
  for (auto& p : points_.point_objects()) {
    p.curves_.erase(handle_);
  }
}

void Curve::update() {
  mark_bezier3_dirty();
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));
}

void Curve::clone_to(Curve& obj) const {
  obj.name   = this->name + " Copy";
  obj.object = this->object;

  auto handles = scene_.get().create_many_objs<SceneObject>(Point{}, this->points_.size_unique());
  if (!handles) {
    util::Logger::err("Could not copy curve. Points could not be created.");
    return;
  }

  for (const auto& h : *handles) {
    if (auto opt = scene_.get().arena<SceneObject>().get_obj(h)) {
      auto& p_obj = **opt;
      p_obj.curves_.insert(obj.handle());
    }
  }

  obj.points_      = this->points_;
  auto old_handles = this->points_.unique_point_handles();
  for (const auto& h : old_handles) {
    if (auto opt1 = scene().arena<SceneObject>().get_obj(h)) {
      if (auto opt2 = scene_.get().arena<SceneObject>().get_obj(handles->back())) {
        const auto& obj1 = **opt1;
        auto& obj2       = **opt2;

        obj1.clone_to(obj2);
        if (!obj.points_.replace(h, obj2)) {
          util::Logger::err("Could not replace handle.");
        }
      }
    }
    handles->pop_back();
  }

  obj.update();
}

std::expected<void, Curve::SceneObjectError> Curve::push_back(const SceneObjectHandle& handle) {
  if (auto o = scene().arena<SceneObject>().get_obj(handle)) {
    auto& obj = *o.value();

    if (std::holds_alternative<Point>(obj.object)) {
      auto result = points_.push_back(obj);
      if (!result) {
        return std::unexpected(static_cast<SceneObjectError>(result.error()));
      }

      obj.curves_.insert(handle_);

      if (std::holds_alternative<BSplineCurve>(this->object)) {
        std::get<BSplineCurve>(this->object).reset_bernstein_points(*this);
        scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::UpdateHelperPoints{}));
      }
      std::visit(eray::util::match{[&](auto& o) { o.on_point_remove(*this, obj, obj.unsafe_get_variant<Point>()); }},
                 this->object);
      scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

      mark_bezier3_dirty();
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
std::expected<void, Curve::SceneObjectError> Curve::move_before(size_t dest_idx, size_t source_idx) {
  auto result = points_.move_before(dest_idx, source_idx);
  if (!result) {
    return std::unexpected(static_cast<SceneObjectError>(result.error()));
  }

  std::visit(eray::util::match{[&](auto& o) { o.on_curve_reorder(*this); }}, this->object);
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

  mark_bezier3_dirty();
  return {};
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
      std::visit(eray::util::match{[&](auto& o) { o.on_point_remove(*this, obj, obj.unsafe_get_variant<Point>()); }},
                 this->object);
      scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

      mark_bezier3_dirty();
      return {};
    }

    return std::unexpected(SceneObjectError::NotAPoint);
  }

  return std::unexpected(SceneObjectError::InvalidHandle);
}

std::expected<void, Curve::SceneObjectError> Curve::move_after(size_t dest_idx, size_t source_idx) {
  auto result = points_.move_before(dest_idx, source_idx);
  if (!result) {
    return std::unexpected(static_cast<SceneObjectError>(result.error()));
  }

  std::visit(eray::util::match{[&](auto& o) { o.on_curve_reorder(*this); }}, this->object);
  scene().renderer().push_object_rs_cmd(CurveRSCommand(handle_, CurveRSCommand::Internal::UpdateControlPoints{}));

  mark_bezier3_dirty();
  return {};
}

void Curve::refresh_bezier3_if_dirty() {
  if (bezier_dirty_) {
    bezier3_points_.clear();
    auto bezier3_points =
        std::visit(eray::util::match{[&](const auto& o) { return o.bezier3_points(*this); }}, this->object);
    for (const auto& p : bezier3_points) {
      bezier3_points_.push_back(p);
    }
    bezier_dirty_ = false;
  }
}

const std::vector<eray::math::Vec3f>& Curve::bezier3_points() {
  refresh_bezier3_if_dirty();
  return bezier3_points_;
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

void BSplineCurve::update_bernstein_points(const Curve& base, const SceneObjectHandle& handle) {
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

  p0.update();
  p1.update();
  p2.update();
  p3.update();

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
    segments_[0].b            = p1;
    segments_[0].c            = p1;
    segments_[0].d            = p1;
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

  // Convert the power basis to the Bezier basis
  for (auto& s : segments_) {
    auto b = s.b * s.chord_length;
    auto c = s.c * s.chord_length * s.chord_length;
    auto d = s.d * s.chord_length * s.chord_length * s.chord_length;

    s.b = s.a + 1.F / 3.F * b;
    s.c = s.a + 2.F / 3.F * b + 1.F / 3.F * c;
    s.d = s.a + b + c + d;
  }
}

void NaturalSplineCurve::update(Curve& base) { reset_segments(base); }

std::generator<eray::math::Vec3f> NaturalSplineCurve::bezier3_points(ref<const Curve> /*base*/) const {
  for (const auto& s : segments_) {
    co_yield s.a;
    co_yield s.b;
    co_yield s.c;
    co_yield s.d;
  }
}

size_t NaturalSplineCurve::bezier3_points_count(ref<const Curve> /*base*/) const { return segments_.size() * 4; }

eray::math::Vec3f Curve::evaluate(float t) {
  refresh_bezier3_if_dirty();
  if (bezier3_points_.empty() || t > 1.F) {
    return math::Vec3f::filled(0.F);
  }

  auto segments_count = bezier3_points_.size() / 4;
  auto segment_len    = 1.F / static_cast<float>(segments_count);
  auto segment_ind    = static_cast<size_t>(t / segment_len);

  // Map to to the segment
  t = (t - static_cast<float>(segment_ind) * segment_len) / segment_len;

  auto i         = segment_ind * 4U;
  const auto& p0 = bezier3_points_[i];
  ++i;
  const auto& p1 = i >= bezier3_points_.size() ? p0 : bezier3_points_[i];
  ++i;
  const auto& p2 = i >= bezier3_points_.size() ? p1 : bezier3_points_[i];
  ++i;
  const auto& p3 = i >= bezier3_points_.size() ? p2 : bezier3_points_[i];

  return bezier3(p0, p1, p2, p3, t);
}

eray::math::Mat4f Curve::frenet_frame(float t) {
  refresh_bezier3_if_dirty();
  if (bezier3_points_.empty()) {
    return math::Mat4f::identity();
  }

  auto segments_count = static_cast<int>(bezier3_points_.size()) / 4;
  auto segment_len    = 1.F / static_cast<float>(segments_count);
  auto segment_ind    = static_cast<size_t>(std::clamp(static_cast<int>(t / segment_len), 0, segments_count - 1));

  // Map to to the segment
  t = (t - static_cast<float>(segment_ind) * segment_len) / segment_len;

  auto i         = segment_ind * 4U;
  const auto& p0 = bezier3_points_[i];
  ++i;
  const auto& p1 = i >= bezier3_points_.size() ? p0 : bezier3_points_[i];
  ++i;
  const auto& p2 = i >= bezier3_points_.size() ? p1 : bezier3_points_[i];
  ++i;
  const auto& p3 = i >= bezier3_points_.size() ? p2 : bezier3_points_[i];

  auto val     = bezier3(p0, p1, p2, p3, t);
  auto val_dt  = bezier3_dt(p0, p1, p2, p3, t);
  auto val_dtt = bezier3_dtt(p0, p1, p2, p3, t);

  // Frenet basis evaluation for regular curve parameterisation
  auto tangent  = math::normalize(val_dt);
  auto binormal = math::normalize(math::cross(val_dt, val_dtt));
  auto normal   = math::normalize(math::cross(binormal, tangent));

  return math::Mat4f{math::Vec4f(tangent, 1.F), math::Vec4f(normal, 1.F), math::Vec4f(binormal, 1.F),
                     math::Vec4f(val, 1.F)};
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> Curve::aabb_bounding_box() {
  static constexpr auto kFltMin = std::numeric_limits<float>::min();
  static constexpr auto kFltMax = std::numeric_limits<float>::max();

  auto min = eray::math::Vec3f::filled(kFltMax);
  auto max = eray::math::Vec3f::filled(kFltMin);

  for (const auto& p : bezier3_points_) {
    min = eray::math::min(p, min);
    max = eray::math::max(p, max);
  }

  return std::make_pair(std::move(min), std::move(max));
}

}  // namespace mini
