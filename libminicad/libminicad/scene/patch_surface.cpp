#include <liberay/util/logger.hpp>
#include <libminicad/math/bezier3.hpp>
#include <libminicad/scene/curve.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <libminicad/scene/trimming.hpp>
#include <vector>

#include "libminicad/renderer/scene_renderer.hpp"

namespace mini {

namespace util = eray::util;
namespace math = eray::math;

void PatchSurface::init_cylinder_from_curve(const CurveHandle& handle, CylinderPatchSurfaceStarter starter,
                                            eray::math::Vec2u dim) {
  if (!has_type<BPatches>()) {
    util::Logger::err("Not implemented.");
    return;
  }
  // TODO(migoox): DRY

  dim.x = std::max(dim.x, 1U);
  dim.y = std::max(dim.y, 1U);

  clear();
  dim_        = dim;
  tess_level_ = kDefaultTessLevel;

  auto points_dim = std::visit(eray::util::match{[&](auto& obj) { return obj.control_points_dim(dim); }}, this->object);
  auto unique_points_dim = std::visit(
      eray::util::match{[&](auto& obj) { return obj.unique_control_points_dim(starter, dim); }}, this->object);

  auto unique_point_handles = scene().create_many_objs<SceneObject>(Point{}, unique_points_dim.x * unique_points_dim.y);
  if (!unique_point_handles) {
    util::Logger::err("Could not create new point objects");
    return;
  }

  auto handles = std::vector<SceneObjectHandle>(points_dim.y * points_dim.x, SceneObjectHandle(0, 0, 0));
  for (auto i = 0U; i < points_dim.y; ++i) {
    for (auto j = 0U; j < points_dim.x; ++j) {
      auto idx        = points_dim.x * i + j;
      auto unique_idx = unique_points_dim.x * (i % unique_points_dim.y) + (j % unique_points_dim.x);
      handles[idx]    = unique_point_handles->at(unique_idx);
    }
  }

  points_.unsafe_set(scene(), handles);

  if (auto opt = scene().arena<Curve>().get_obj(handle)) {
    auto& curve = **opt;
    auto n      = dim_.x * 2;
    auto nf     = static_cast<float>(n);

    auto alpha      = std::numbers::pi_v<float> * 2.F / nf;
    auto alpha_half = alpha / 2.F;
    auto beta       = std::numbers::pi_v<float> * (nf - 2.F) / (2.F * nf);
    auto gamma      = std::numbers::pi_v<float> - 2.F * beta;
    auto r          = starter.radius * (1.F + std::tan(alpha_half) * std::tan(gamma));

    for (auto y = 0U; y < points_dim.y; ++y) {
      auto x_idx = 0U;

      auto t         = static_cast<float>(y) / static_cast<float>(points_dim.y);
      auto frame_mat = curve.frenet_frame(t);

      auto p = eray::math::Vec3f::filled(0.F);
      for (auto x = 0U; x < points_dim.x; ++x) {
        auto curr_alpha = static_cast<float>(x) * 2.F * alpha;
        p.x             = 0.F;
        p.y             = std::cos(curr_alpha + starter.phase) * r;
        p.z             = std::sin(curr_alpha + starter.phase) * r;

        p = math::Vec3f(frame_mat * eray::math::Vec4f(p, 1.F));

        auto idx = points_dim.x * y + x_idx;
        points_.unsafe_by_idx(idx).transform.set_local_pos(p);
        x_idx++;
      }
    }
  }

  for (auto& p : points_.point_objects()) {
    p.patch_surfaces_.insert(handle_);
    p.update();
  }
}

PatchSurface::PatchSurface(const PatchSurfaceHandle& handle, Scene& scene)
    : ObjectBase<PatchSurface, PatchSurfaceVariant>(handle, scene),
      trimming_manager_(ParamSpaceTrimmingDataManager::create(IntersectionFinder::Curve::kTxtSize,
                                                              IntersectionFinder::Curve::kTxtSize)),
      txt_handle_(TextureHandle(0, 0, 0)) {
  txt_handle_ = scene_.get().renderer().upload_texture(trimming_manager_.final_txt(), trimming_manager_.width(),
                                                       trimming_manager_.height());
  scene_.get().renderer().push_object_rs_cmd(
      PatchSurfaceRSCommand(handle, PatchSurfaceRSCommand::Internal::AddObject{}));
}

void PatchSurface::init_from_starter(const PatchSurfaceStarter& starter, eray::math::Vec2u dim) {
  dim.x = std::max(dim.x, 1U);
  dim.y = std::max(dim.y, 1U);

  clear();
  dim_        = dim;
  tess_level_ = kDefaultTessLevel;

  auto points_dim = std::visit(eray::util::match{[&](auto& obj) { return obj.control_points_dim(dim); }}, this->object);
  auto unique_points_dim = std::visit(
      eray::util::match{[&](auto& obj) { return obj.unique_control_points_dim(starter, dim); }}, this->object);

  auto unique_point_handles = scene().create_many_objs<SceneObject>(Point{}, unique_points_dim.x * unique_points_dim.y);
  if (!unique_point_handles) {
    util::Logger::err("Could not create new point objects");
    return;
  }

  auto handles = std::vector<SceneObjectHandle>(points_dim.y * points_dim.x, SceneObjectHandle(0, 0, 0));
  for (auto i = 0U; i < points_dim.y; ++i) {
    for (auto j = 0U; j < points_dim.x; ++j) {
      auto idx        = points_dim.x * i + j;
      auto unique_idx = unique_points_dim.x * (i % unique_points_dim.y) + (j % unique_points_dim.x);
      handles[idx]    = unique_point_handles->at(unique_idx);
    }
  }

  points_.unsafe_set(scene(), handles);

  std::visit(eray::util::match{[&](auto& obj) { obj.set_control_points(points_, starter, dim); }}, this->object);

  for (auto& p : points_.point_objects()) {
    p.patch_surfaces_.insert(handle_);
    p.update();
  }
}

std::expected<void, PatchSurface::InitError> PatchSurface::init_from_points(
    eray::math::Vec2u points_dim, const std::vector<SceneObjectHandle>& points) {
  if (points_dim.x * points_dim.y != points.size()) {
    return std::unexpected(InitError::PointsAndDimensionsMismatch);
  }

  dim_ = std::visit(eray::util::match{[&](auto& obj) { return obj.patches_dim(points_dim); }}, this->object);
  if (dim_.x < 1U || dim_.y < 1U) {
    return std::unexpected(InitError::NonPositiveDimensions);
  }

  for (const auto& h : points) {
    if (auto opt = scene().arena<SceneObject>().get_obj(h)) {
      auto& obj = **opt;
      if (obj.has_type<Point>()) {
        obj.patch_surfaces_.insert(handle_);
      } else {
        util::Logger::err("One of the provided scene object is not a point");
        return std::unexpected(InitError::SceneObjectIsNotAPoint);
      }
    } else {
      util::Logger::err("One of the provided scene object does not exist");
      return std::unexpected(InitError::SceneObjectDoesNotExist);
    }
  }

  clear();
  tess_level_ = kDefaultTessLevel;
  points_.unsafe_set(scene(), points);

  return {};
}

void PatchSurface::clear() {
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
}

void BezierPatches::set_control_points(PointList& points, const PatchSurfaceStarter& starter, eray::math::Vec2u dim) {
  if (std::holds_alternative<PlanePatchSurfaceStarter>(starter)) {
    auto s = std::get<PlanePatchSurfaceStarter>(starter);

    for (auto col = 0U; col < dim.x; ++col) {
      for (auto row = 0U; row < dim.y; ++row) {
        auto start_in_col = col == 0 ? 0U : 1U;
        auto start_in_row = row == 0 ? 0U : 1U;
        for (auto in_col = start_in_col; in_col < PatchSurface::kPatchSize; ++in_col) {
          for (auto in_row = start_in_row; in_row < PatchSurface::kPatchSize; ++in_row) {
            auto p = eray::math::Vec3f::filled(0.F);
            p.x    = s.size.x * static_cast<float>(col * (PatchSurface::kPatchSize - 1) + in_col) /
                  static_cast<float>(dim.x * (PatchSurface::kPatchSize - 1) + 1);
            p.z = s.size.y * static_cast<float>(row * (PatchSurface::kPatchSize - 1) + in_row) /
                  static_cast<float>(dim.y * (PatchSurface::kPatchSize - 1) + 1);
            auto idx = find_idx(col, row, in_col, in_row, dim.x);

            points.unsafe_by_idx(idx).transform.set_local_pos(p);
          }
        }
      }
    }

  } else if (std::holds_alternative<CylinderPatchSurfaceStarter>(starter)) {
    auto s = std::get<CylinderPatchSurfaceStarter>(starter);
    auto n = dim.x * 2;

    auto alpha      = std::numbers::pi_v<float> * 2.F / static_cast<float>(n);
    auto alpha_half = alpha / 2.F;
    auto inner_r    = s.radius;
    auto outer_r    = s.radius / std::cos(alpha_half);

    auto points_dim = control_points_dim(dim);

    for (auto y = 0U; y < points_dim.y; ++y) {
      auto x_idx = 0U;
      auto p     = eray::math::Vec3f::filled(0.F);
      p.y        = s.height * static_cast<float>(y) / static_cast<float>(points_dim.y);
      for (auto x = 0U; x < n; ++x) {
        auto curr_alpha = static_cast<float>(x) * alpha;

        if (x % 2 == 0 && x < n) {
          p.x = std::cos(curr_alpha) * inner_r;
          p.z = std::sin(curr_alpha) * inner_r;

          auto idx = points_dim.x * y + x_idx;
          points.unsafe_by_idx(idx).transform.set_local_pos(p);
          x_idx++;
        }

        p.x = std::cos(curr_alpha + alpha_half) * outer_r;
        p.z = std::sin(curr_alpha + alpha_half) * outer_r;

        auto idx = points_dim.x * y + x_idx;

        points.unsafe_by_idx(idx).transform.set_local_pos(p);
        x_idx++;
      }
    }
  }
}

eray::math::Vec2u BezierPatches::control_points_dim(eray::math::Vec2u dim) {
  return eray::math::Vec2u(dim.x * (PatchSurface::kPatchSize - 1) + 1, dim.y * (PatchSurface::kPatchSize - 1) + 1);
}

eray::math::Vec2u BezierPatches::patches_dim(eray::math::Vec2u points_dim) {
  return eray::math::Vec2u(points_dim.x / 3, points_dim.y / 3);
}

void PatchSurface::insert_row_top() { util::Logger::err("Not implemented"); }

void PatchSurface::insert_row_bottom() {
  auto insert_bpatches = [&](const BPatches&) {
    const auto points_x = dim_.x + 3;
    const auto points_y = dim_.y + 3;

    auto opt_points = scene().create_many_objs<SceneObject>(Point{}, points_x);
    if (!opt_points) {
      util::Logger::err("Failed to insert row");
      return;
    }

    auto all_handles        = points_.point_handles() | std::ranges::to<std::vector>();
    const auto& new_handles = *opt_points;
    for (auto i = 0U; i < points_x; ++i) {
      if (auto opt_up = scene().arena<SceneObject>().get_obj(all_handles[points_x * (points_y - 2) + i])) {
        if (auto opt = scene().arena<SceneObject>().get_obj(all_handles[points_x * (points_y - 1) + i])) {
          auto& old_obj_up = **opt_up;
          auto& old_obj    = **opt;

          auto p0 = old_obj_up.transform.pos();
          auto p1 = old_obj.transform.pos();

          auto& new_obj = **scene().arena<SceneObject>().get_obj(new_handles[i]);
          new_obj.patch_surfaces_.insert(handle_);
          new_obj.transform.set_local_pos(2.F * p1 - p0);
          new_obj.update();
        }
      }
    }

    all_handles.insert(all_handles.end(), new_handles.begin(), new_handles.end());
    points_.unsafe_set(scene_.get(), all_handles);
    ++dim_.y;
    update();
  };

  auto insert_bezier_patches = [&](const BezierPatches&) { util::Logger::err("Not implemented"); };

  std::visit(eray::util::match{insert_bpatches, insert_bezier_patches}, object);
}

void PatchSurface::insert_column_left() { util::Logger::err("Not implemented"); }

void PatchSurface::insert_column_right() { util::Logger::err("Not implemented"); }

void PatchSurface::delete_row_top() {
  auto delete_bpatches = [&](const BPatches&) {
    const auto points_x = dim_.x + 3;

    auto handles_to_remove = std::vector<SceneObjectHandle>();
    handles_to_remove.reserve(points_x);
    for (auto i = 0U; i < points_x; ++i) {
      auto& obj = points_.unsafe_by_idx(i);
      obj.patch_surfaces_.erase(handle());
      handles_to_remove.push_back(obj.handle());
    }
    points_.remove_many(handles_to_remove);
    --dim_.y;
    update();
  };

  auto delete_bezier_patches = [&](const BezierPatches&) { util::Logger::err("Not implemented"); };

  std::visit(eray::util::match{delete_bpatches, delete_bezier_patches}, object);
}

void PatchSurface::delete_row_bottom() {
  auto delete_bpatches = [&](const BPatches&) {
    const auto points_x = dim_.x + 3;
    const auto points_y = dim_.y + 3;

    auto handles_to_remove = std::vector<SceneObjectHandle>();
    handles_to_remove.reserve(points_x);
    for (auto i = 0U; i < points_x; ++i) {
      auto& obj = points_.unsafe_by_idx(points_x * (points_y - 1) + i);
      obj.patch_surfaces_.erase(handle());
      handles_to_remove.push_back(obj.handle());
    }
    points_.remove_many(handles_to_remove);
    --dim_.y;
    update();
  };

  auto delete_bezier_patches = [&](const BezierPatches&) { util::Logger::err("Not implemented"); };

  std::visit(eray::util::match{delete_bpatches, delete_bezier_patches}, object);
}

void PatchSurface::delete_column_left() { util::Logger::err("Not implemented"); }

void PatchSurface::delete_column_right() { util::Logger::err("Not implemented"); }

eray::math::Vec2u BezierPatches::unique_control_points_dim(const PatchSurfaceStarter& starter, eray::math::Vec2u dim) {
  return std::visit(eray::util::match{[&](const PlanePatchSurfaceStarter&) {
                                        return eray::math::Vec2u(dim.x * (PatchSurface::kPatchSize - 1) + 1,
                                                                 dim.y * (PatchSurface::kPatchSize - 1) + 1);
                                      },
                                      [&](const CylinderPatchSurfaceStarter&) {
                                        return eray::math::Vec2u(
                                            dim.x * (PatchSurface::kPatchSize - 1),  // last column = first column
                                            dim.y * (PatchSurface::kPatchSize - 1) + 1);
                                      }},
                    starter);
}

const std::vector<eray::math::Vec3f>& PatchSurface::bezier3_points() {
  if (bezier_dirty_) {
    std::visit(eray::util::match{[this](auto& type) { return type.update_bezier3_points(*this); }}, this->object);
    bezier_dirty_ = false;
  }

  return bezier3_points_;
}

std::generator<eray::math::Vec3f> PatchSurface::control_grid_points() const {
  if (dim_.x <= 0 || dim_.x <= 0) {
    co_return;
  }
  auto dim =
      std::visit(eray::util::match{[this](const auto& type) { return type.control_points_dim(dim_); }}, this->object);

  for (auto row = 0U; row < dim.y; ++row) {
    for (auto col = 0U; col < dim.x - 1; ++col) {
      co_yield points_.unsafe_by_idx(dim.x * row + col).transform.pos();
      co_yield points_.unsafe_by_idx(dim.x * row + col + 1).transform.pos();
    }
  }

  for (auto row = 0U; row < dim.y - 1; ++row) {
    for (auto col = 0U; col < dim.x; ++col) {
      co_yield points_.unsafe_by_idx(dim.x * row + col).transform.pos();
      co_yield points_.unsafe_by_idx(dim.x * (row + 1) + col).transform.pos();
    }
  }
}

size_t PatchSurface::control_grid_points_count() const {
  auto dim =
      std::visit(eray::util::match{[this](const auto& type) { return type.control_points_dim(dim_); }}, this->object);

  return 2 * ((dim.y - 1) * dim.x + dim.y * (dim.x - 1));
}

void PatchSurface::set_tess_level(int tesselation) {
  // fix tesselation
  auto st     = static_cast<int>(std::sqrt(tesselation));
  tesselation = st * st;

  tess_level_ = tesselation;

  update();
}

void PatchSurface::on_delete() {
  scene().renderer().push_object_rs_cmd(
      PatchSurfaceRSCommand(handle_, PatchSurfaceRSCommand::Internal::DeleteObject{}));
  for (auto& p : points_.point_objects()) {
    p.patch_surfaces_.erase(handle_);
  }

  for (auto it = fill_in_surfaces_.begin(); it != fill_in_surfaces_.end();) {
    auto fs_h = *it;
    it        = fill_in_surfaces_.erase(it);
    scene().delete_obj(fs_h);
  }
}

void BezierPatches::update_bezier3_points(PatchSurface& base) {
  if (base.dim_.x == 0 || base.dim_.y == 0) {
    base.bezier3_points_.resize(0);
    return;
  }

  base.bezier3_points_.resize(base.dim_.x * base.dim_.y * PatchSurface::kPatchSize * PatchSurface::kPatchSize);
  auto idx = 0U;
  for (auto row = 0U; row < base.dim_.y; ++row) {
    for (auto col = 0U; col < base.dim_.x; ++col) {
      for (auto in_row = 0U; in_row < PatchSurface::kPatchSize; ++in_row) {
        for (auto in_col = 0U; in_col < PatchSurface::kPatchSize; ++in_col) {
          base.bezier3_points_[idx++] =
              base.points_.unsafe_by_idx(find_idx(col, row, in_col, in_row, base.dim_.x)).transform.pos();
        }
      }
    }
  }
}

size_t BezierPatches::find_idx(size_t patch_x, size_t patch_y, size_t point_x, size_t point_y, size_t dim_x) {
  return ((PatchSurface::kPatchSize - 1) * dim_x + 1) * ((PatchSurface::kPatchSize - 1) * patch_y + point_y) +
         ((PatchSurface::kPatchSize - 1) * patch_x + point_x);
}

size_t BezierPatches::find_patch_offset(size_t patch_x, size_t patch_y, size_t dim_x) {
  auto size_x = dim_x * (PatchSurface::kPatchSize - 1) + 1U;
  return patch_y * size_x * (PatchSurface::kPatchSize - 1) + patch_x * (PatchSurface::kPatchSize - 1);
}

size_t BezierPatches::find_size_x(size_t dim_x) { return dim_x * (PatchSurface::kPatchSize - 1) + 1U; }

void BPatches::set_control_points(PointList& points, const PatchSurfaceStarter& starter, eray::math::Vec2u dim) {
  std::visit(
      eray::util::match{
          [&](const PlanePatchSurfaceStarter&) {
            auto s = std::get<PlanePatchSurfaceStarter>(starter);
            for (auto col = 0U; col < dim.x + PatchSurface::kPatchSize - 1; ++col) {
              for (auto row = 0U; row < dim.y + PatchSurface::kPatchSize - 1; ++row) {
                auto idx = row * (dim.x + PatchSurface::kPatchSize - 1) + col;
                auto p   = eray::math::Vec3f::filled(0.F);
                p.x = s.size.x * static_cast<float>(col) / static_cast<float>(dim.x + PatchSurface::kPatchSize - 1);
                p.z = s.size.y * static_cast<float>(row) / static_cast<float>(dim.y + PatchSurface::kPatchSize - 1);
                points.unsafe_by_idx(idx).transform.set_local_pos(p);
              }
            }
          },
          [&](const CylinderPatchSurfaceStarter&) {
            auto s  = std::get<CylinderPatchSurfaceStarter>(starter);
            auto n  = dim.x * 2;
            auto nf = static_cast<float>(n);

            auto alpha      = std::numbers::pi_v<float> * 2.F / nf;
            auto alpha_half = alpha / 2.F;
            auto beta       = std::numbers::pi_v<float> * (nf - 2.F) / (2.F * nf);
            auto gamma      = std::numbers::pi_v<float> - 2.F * beta;
            auto r          = s.radius * (1.F + std::tan(alpha_half) * std::tan(gamma));

            auto points_dim = control_points_dim(dim);

            for (auto y = 0U; y < points_dim.y; ++y) {
              auto x_idx = 0U;
              auto p     = eray::math::Vec3f::filled(0.F);
              p.y        = s.height * static_cast<float>(y) / static_cast<float>(points_dim.y);
              for (auto x = 0U; x < points_dim.x; ++x) {
                auto curr_alpha = static_cast<float>(x) * 2.F * alpha;
                p.x             = std::cos(curr_alpha) * r;
                p.z             = std::sin(curr_alpha) * r;

                auto idx = points_dim.x * y + x_idx;
                points.unsafe_by_idx(idx).transform.set_local_pos(p);
                x_idx++;
              }
            }
          },

      },
      starter);
}

eray::math::Vec2u BPatches::control_points_dim(eray::math::Vec2u dim) {
  return eray::math::Vec2u(dim.x + PatchSurface::kPatchSize - 1, dim.y + PatchSurface::kPatchSize - 1);
}

eray::math::Vec2u BPatches::patches_dim(eray::math::Vec2u points_dim) {
  return eray::math::Vec2u(points_dim.x - PatchSurface::kPatchSize + 1, points_dim.y - PatchSurface::kPatchSize + 1);
}

eray::math::Vec2u BPatches::unique_control_points_dim(const PatchSurfaceStarter& starter, eray::math::Vec2u dim) {
  return std::visit(eray::util::match{[&](const PlanePatchSurfaceStarter&) {
                                        return eray::math::Vec2u(dim.x + PatchSurface::kPatchSize - 1,
                                                                 dim.y + PatchSurface::kPatchSize - 1);
                                      },
                                      [&](const CylinderPatchSurfaceStarter&) {
                                        return eray::math::Vec2u(dim.x, dim.y + PatchSurface::kPatchSize - 1);
                                      }},
                    starter);
}

void BPatches::update_bezier3_points(PatchSurface& base) {
  auto dim = base.dimensions();
  if (base.dim_.x == 0 || base.dim_.y == 0) {
    base.bezier3_points_.resize(0);
    return;
  }

  base.bezier3_points_.resize(base.dim_.x * base.dim_.y * PatchSurface::kPatchSize * PatchSurface::kPatchSize);

  math::Vec3f bezier_patch[4][4];
  auto idx = 0U;
  for (auto x = 0U; x < dim.x; ++x) {
    for (auto y = 0U; y < dim.y; ++y) {
      for (auto iy = 0U; iy < PatchSurface::kPatchSize; ++iy) {
        auto p0 = base.points_.unsafe_by_idx(find_idx(x, y, 0, iy, dim.x)).transform.pos();
        auto p1 = base.points_.unsafe_by_idx(find_idx(x, y, 1, iy, dim.x)).transform.pos();
        auto p2 = base.points_.unsafe_by_idx(find_idx(x, y, 2, iy, dim.x)).transform.pos();
        auto p3 = base.points_.unsafe_by_idx(find_idx(x, y, 3, iy, dim.x)).transform.pos();

        bezier_patch[iy][0] = (p0 + 4 * p1 + p2) / 6.0;
        bezier_patch[iy][1] = (4 * p1 + 2 * p2) / 6.0;
        bezier_patch[iy][2] = (2 * p1 + 4 * p2) / 6.0;
        bezier_patch[iy][3] = (p1 + 4 * p2 + p3) / 6.0;
      }
      for (auto ix = 0U; ix < PatchSurface::kPatchSize; ++ix) {
        auto c0 = bezier_patch[0][ix];
        auto c1 = bezier_patch[1][ix];
        auto c2 = bezier_patch[2][ix];
        auto c3 = bezier_patch[3][ix];

        base.bezier3_points_[idx++] = (c0 + 4.0 * c1 + c2) / 6.0;
        base.bezier3_points_[idx++] = (4.0 * c1 + 2.0 * c2) / 6.0;
        base.bezier3_points_[idx++] = (2.0 * c1 + 4.0 * c2) / 6.0;
        base.bezier3_points_[idx++] = (c1 + 4.0 * c2 + c3) / 6.0;
      }
    }
  }
}

size_t BPatches::find_idx(size_t patch_x, size_t patch_y, size_t point_x, size_t point_y, size_t dim_x) {
  return (patch_y + point_y) * (dim_x + PatchSurface::kPatchSize - 1) + (patch_x + point_x);
}

size_t BPatches::find_patch_offset(size_t patch_x, size_t patch_y, size_t dim_x) {
  return patch_y * (dim_x + PatchSurface::kPatchSize - 1) + patch_x;
}

size_t BPatches::find_size_x(size_t dim_x) { return dim_x + PatchSurface::kPatchSize - 1U; }

eray::math::Vec2u PatchSurface::control_points_dim() const {
  return std::visit(eray::util::match{[&](const auto& v) { return v.control_points_dim(dim_); }}, object);
}

void PatchSurface::update() {
  mark_bezier3_dirty();
  scene().renderer().push_object_rs_cmd(
      PatchSurfaceRSCommand(handle(), PatchSurfaceRSCommand::Internal::UpdateControlPoints{}));
}

std::expected<std::array<std::array<SceneObjectHandle, PatchSurface::kPatchSize>, PatchSurface::kPatchSize>,
              PatchSurface::GetterError>
PatchSurface::patch_control_point_handles(eray::math::Vec2u patch_coords) const {
  if (patch_coords.x > dim_.x || patch_coords.y > dim_.y) {
    return std::unexpected(GetterError::OutOfBounds);
  }
  return unsafe_patch_control_point_handles(patch_coords);
}

std::array<std::array<SceneObjectHandle, PatchSurface::kPatchSize>, PatchSurface::kPatchSize>
PatchSurface::unsafe_patch_control_point_handles(eray::math::Vec2u patch_coords) const {
  auto size_x =
      std::visit(eray::util::match{[&](const auto& variant) { return variant.find_size_x(dim_.x); }}, this->object);
  auto offset = std::visit(eray::util::match{[&](const auto& variant) {
                             return variant.find_patch_offset(patch_coords.x, patch_coords.y, dim_.x);
                           }},
                           this->object);

  return {{{{
               points_.unsafe_by_idx(offset + size_x * 0 + 0).handle(),
               points_.unsafe_by_idx(offset + size_x * 0 + 1).handle(),
               points_.unsafe_by_idx(offset + size_x * 0 + 2).handle(),
               points_.unsafe_by_idx(offset + size_x * 0 + 3).handle(),
           }},
           {{
               points_.unsafe_by_idx(offset + size_x * 1 + 0).handle(),
               points_.unsafe_by_idx(offset + size_x * 1 + 1).handle(),
               points_.unsafe_by_idx(offset + size_x * 1 + 2).handle(),
               points_.unsafe_by_idx(offset + size_x * 1 + 3).handle(),
           }},
           {{
               points_.unsafe_by_idx(offset + size_x * 2 + 0).handle(),
               points_.unsafe_by_idx(offset + size_x * 2 + 1).handle(),
               points_.unsafe_by_idx(offset + size_x * 2 + 2).handle(),
               points_.unsafe_by_idx(offset + size_x * 2 + 3).handle(),
           }},
           {{
               points_.unsafe_by_idx(offset + size_x * 3 + 0).handle(),
               points_.unsafe_by_idx(offset + size_x * 3 + 1).handle(),
               points_.unsafe_by_idx(offset + size_x * 3 + 2).handle(),
               points_.unsafe_by_idx(offset + size_x * 3 + 3).handle(),
           }}}};
}

void PatchSurface::clone_to(PatchSurface& obj) const {
  obj.name        = this->name + " Copy";
  obj.object      = this->object;
  obj.dim_        = this->dim_;
  obj.tess_level_ = this->tess_level_;

  auto handles = scene_.get().create_many_objs<SceneObject>(Point{}, this->points_.size_unique());
  if (!handles) {
    util::Logger::err("Could not copy curve. Points could not be created.");
    return;
  }

  for (const auto& h : *handles) {
    if (auto opt = scene_.get().arena<SceneObject>().get_obj(h)) {
      auto& p_obj = **opt;
      p_obj.patch_surfaces_.insert(obj.handle());
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

std::pair<eray::math::Vec2f, eray::math::Vec2u> PatchSurface::find_bezier3_patch_and_param(float u, float v) {
  auto param = eray::math::Vec2f(u, v);

  auto patch_size   = 1.F / eray::math::Vec2f(static_cast<float>(dim_.x), static_cast<float>(dim_.y));
  auto patch_coords = param / patch_size;

  const auto coord_x =
      static_cast<uint32_t>(std::clamp(static_cast<int>(patch_coords.x), 0, static_cast<int>(dim_.x) - 1));
  const auto coord_y =
      static_cast<uint32_t>(std::clamp(static_cast<int>(patch_coords.y), 0, static_cast<int>(dim_.y) - 1));

  patch_coords = eray::math::Vec2f(static_cast<float>(coord_x), static_cast<float>(coord_y));

  // Map to to the patch
  param = (param - patch_coords * patch_size) / patch_size;

  return std::make_pair(param, math::Vec2u(coord_x, coord_y));
}

eray::math::Vec3f PatchSurface::evaluate(float u, float v) {
  auto points = bezier3_points();
  if (points.empty()) {
    return math::Vec3f::zeros();
  }

  auto [param, patch_coords] = find_bezier3_patch_and_param(u, v);

  auto pu  = std::array<math::Vec3f, kPatchSize>();
  auto idx = kPatchSize * kPatchSize * dim_.x * patch_coords.y + kPatchSize * kPatchSize * patch_coords.x;
  for (auto i = 0U; i < kPatchSize; ++i) {
    auto curr_row_idx = idx + kPatchSize * i;
    pu[i] = bezier3(points[curr_row_idx], points[curr_row_idx + 1], points[curr_row_idx + 2], points[curr_row_idx + 3],
                    param.x);
  }
  return bezier3(pu[0], pu[1], pu[2], pu[3], param.y);
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> PatchSurface::evaluate_derivatives(float u, float v) {
  auto points = bezier3_points();
  if (points.empty()) {
    return std::make_pair(math::Vec3f::zeros(), math::Vec3f::zeros());
  }

  auto [param, patch_coords] = find_bezier3_patch_and_param(u, v);

  auto pu  = std::array<math::Vec3f, kPatchSize>();
  auto pv  = std::array<math::Vec3f, kPatchSize>();
  auto idx = kPatchSize * kPatchSize * dim_.x * patch_coords.y + kPatchSize * kPatchSize * patch_coords.x;
  for (auto i = 0U; i < kPatchSize; ++i) {
    auto pu_idx = idx + kPatchSize * i;
    pu[i]       = bezier3(points[pu_idx], points[pu_idx + 1], points[pu_idx + 2], points[pu_idx + 3], param.x);

    auto pv_idx = idx + i;
    pv[i]       = bezier3(points[pv_idx], points[pv_idx + kPatchSize], points[pv_idx + 2 * kPatchSize],
                          points[pv_idx + 3 * kPatchSize], param.y);
  }

  return std::make_pair(bezier3_dt(pv[0], pv[1], pv[2], pv[3], param.x),
                        bezier3_dt(pu[0], pu[1], pu[2], pu[3], param.y));
}

std::pair<eray::math::Vec3f, eray::math::Vec3f> PatchSurface::aabb_bounding_box() const {
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

void PatchSurface::update_trimming_txt() {
  trimming_manager_.update_final_txt();
  scene().renderer().reupload_texture(txt_handle_, trimming_manager_.final_txt(), trimming_manager_.width(),
                                      trimming_manager_.height());
}

}  // namespace mini
