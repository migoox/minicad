#include <algorithm>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <ranges>
#include <variant>

namespace mini {

namespace util = eray::util;

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

        if (std::holds_alternative<Point>(object)) {
          if (std::holds_alternative<BSplineCurve>(pl.value()->object)) {
            std::get<BSplineCurve>(pl.value()->object).update_bernstein_points(*pl.value());
            scene_.renderer().push_object_rs_cmd(
                PointListObjectRSCommand(pl_h, PointListObjectRSCommand::UpdateBernsteinControlPoints{}));
          }
        }
      }
    }
  }
}

void SceneObject::mark_dirty() {
  scene_.renderer().push_object_rs_cmd(SceneObjectRSCommand(handle_, SceneObjectRSCommand::UpdateObjectMembers{}));

  if (std::holds_alternative<Point>(object)) {
    for (const auto& pl : this->point_lists_) {
      if (auto o = scene_.get_obj(pl)) {
        if (std::holds_alternative<BSplineCurve>(o.value()->object)) {
          std::get<BSplineCurve>(o.value()->object).update_bernstein_points(*o.value());
          scene_.renderer().push_object_rs_cmd(
              PointListObjectRSCommand(pl, PointListObjectRSCommand::UpdateBernsteinControlPoints{}));
        }
      }
    }
  }
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
      std::get<BSplineCurve>(this->object).update_bernstein_points(*this);
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

void BSplineCurve::update_bernstein_points(const PointListObject& base) {
  auto de_boor_points = base.points() |
                        std::ranges::views::transform([](const SceneObject& s) { return s.transform.pos(); }) |
                        std::ranges::views::adjacent<4>;

  bernstein_points_.clear();
  for (const auto& [p0, p1, p2, p3] : de_boor_points) {
    bernstein_points_.push_back((p0 + 4 * p1 + p2) / 6.0);
    bernstein_points_.push_back((4 * p1 + 2 * p2) / 6.0);
    bernstein_points_.push_back((2 * p1 + 4 * p2) / 6.0);
    bernstein_points_.push_back((p1 + 4 * p2 + p3) / 6.0);
  }
}

void BSplineCurve::set_bernstein_point(PointListObject& base, size_t ind, eray::math::Vec3f point) {
  if (bernstein_points_.size() <= ind) {
    return;
  }
  bernstein_points_[ind] = point;

  size_t cp_ind = ind / 4;

  auto de_boor_control_points_windows = base.points() | std::ranges::views::slide(4);
  auto de_boor_points_to_update       = *(de_boor_control_points_windows.begin() + static_cast<int>(cp_ind));
  auto& p0                            = de_boor_points_to_update[0];
  auto& p1                            = de_boor_points_to_update[1];
  auto& p2                            = de_boor_points_to_update[2];
  auto& p3                            = de_boor_points_to_update[3];

  const auto& bp0 = bernstein_points_[4 * cp_ind];
  const auto& bp1 = bernstein_points_[4 * cp_ind + 1];
  const auto& bp2 = bernstein_points_[4 * cp_ind + 2];
  const auto& bp3 = bernstein_points_[4 * cp_ind + 3];

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

  update_bernstein_points(base);
}

}  // namespace mini
