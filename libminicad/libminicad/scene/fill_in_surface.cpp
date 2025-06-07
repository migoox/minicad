#include <algorithm>
#include <liberay/util/container_extensions.hpp>
#include <liberay/util/logger.hpp>
#include <libminicad/scene/fill_in_suface.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini {

FillInSurface::FillInSurface(const FillInSurfaceHandle& handle, Scene& scene)
    : ObjectBase<FillInSurface, FillInSurfaceVariant>(handle, scene), neighbors_([] {
        SurfaceNeighbor default_neighbor{
            .boundaries = eray::util::make_filled_array<std::array<SceneObjectHandle, 4>, 2>(
                eray::util::make_filled_array<SceneObjectHandle, 4>(SceneObjectHandle(0, 0, 0))),
            .handle = PatchSurfaceHandle(0, 0, 0)};

        return eray::util::make_filled_array<SurfaceNeighbor, kNeighbors>(default_neighbor);
      }()) {}

std::expected<void, FillInSurface::InitError> FillInSurface::init(std::array<SurfaceNeighbor, kNeighbors>&& neighbors) {
  for (auto& n : neighbors) {
    for (const auto& b : n.boundaries) {
      for (const auto h : b) {
        if (auto opt = scene().arena<SceneObject>().get_obj(h)) {
          auto& obj = **opt;
          if (!obj.has_type<Point>()) {
            eray::util::Logger::err("Could not create a fill in surface. One of the scene objects is not a point.");
            return std::unexpected(FillInSurface::InitError::NotAPoint);
          }

          obj.fill_in_surfaces_.insert(handle());
        } else {
          eray::util::Logger::err("Could not create a fill in surface. One of the points does not exist.");
          return std::unexpected(FillInSurface::InitError::PointDoesNotExist);
        }
      }
    }

    if (auto opt = scene().arena<PatchSurface>().get_obj(n.handle)) {
      auto& obj = **opt;
      if (!obj.has_type<BezierPatches>()) {
        eray::util::Logger::err(
            "Could not create a fill in surface. One of the surfaces is not a bezier patches surface.");
        return std::unexpected(FillInSurface::InitError::NotABezierPatch);
      }

      obj.fill_in_surfaces_.insert(handle());
    } else {
      eray::util::Logger::err("Could not create a fill in surface. One of the surfaces does not exist.");
      return std::unexpected(FillInSurface::InitError::PatchDoesNotExist);
    }
  }

  neighbors_ = std::move(neighbors);

  // Fix winding order
  auto first_end = neighbors_[0].boundaries[0][3].obj_id;
  if (first_end != neighbors_[1].boundaries[0][0].obj_id && first_end != neighbors_[1].boundaries[0][3].obj_id) {
    std::swap(neighbors_[1], neighbors_[2]);
  }
  if (first_end != neighbors_[1].boundaries[0][0].obj_id) {
    std::ranges::reverse(neighbors_[1].boundaries[0]);
    std::ranges::reverse(neighbors_[1].boundaries[1]);
  }
  if (neighbors_[1].boundaries[0][3].obj_id != neighbors_[2].boundaries[0][0].obj_id) {
    std::ranges::reverse(neighbors_[2].boundaries[0]);
    std::ranges::reverse(neighbors_[2].boundaries[1]);
  }

  scene().renderer().push_object_rs_cmd(
      FillInSurfaceRSCommand(handle(), FillInSurfaceRSCommand::Internal::AddObject()));
  mark_points_dirty();
  return {};
}

std::generator<eray::math::Vec3f> FillInSurface::tangent_grid_points() {
  update();

  // Patch 0
  co_yield rational_bezier_points_[2];
  co_yield rational_bezier_points_[17];

  co_yield rational_bezier_points_[1];
  co_yield rational_bezier_points_[16];

  co_yield rational_bezier_points_[4];
  co_yield rational_bezier_points_[5];

  co_yield rational_bezier_points_[8];
  co_yield rational_bezier_points_[9];

  // Patch 1
  co_yield rational_bezier_points_[20 + 4];
  co_yield rational_bezier_points_[20 + 5];

  co_yield rational_bezier_points_[20 + 8];
  co_yield rational_bezier_points_[20 + 9];

  co_yield rational_bezier_points_[20 + 13];
  co_yield rational_bezier_points_[20 + 18];

  co_yield rational_bezier_points_[20 + 14];
  co_yield rational_bezier_points_[20 + 19];

  // Patch 2
  co_yield rational_bezier_points_[2 * 20 + 7];
  co_yield rational_bezier_points_[2 * 20 + 6];

  co_yield rational_bezier_points_[2 * 20 + 11];
  co_yield rational_bezier_points_[2 * 20 + 10];

  co_yield rational_bezier_points_[2 * 20 + 14];
  co_yield rational_bezier_points_[2 * 20 + 19];

  co_yield rational_bezier_points_[2 * 20 + 13];
  co_yield rational_bezier_points_[2 * 20 + 18];
}

std::generator<eray::math::Vec3f> FillInSurface::control_grid_points() {
  update();
  for (auto i = 0U; i < kNeighbors; ++i) {
    // vertical
    for (auto j = 0U, k = 0U; j < 2U; ++j) {
      co_yield rational_bezier_points_[20 * i + k];
      ++k;
      co_yield rational_bezier_points_[20 * i + k];

      co_yield rational_bezier_points_[20 * i + k];
      ++k;
      co_yield rational_bezier_points_[20 * i + k];

      co_yield rational_bezier_points_[20 * i + k];
      ++k;
      co_yield rational_bezier_points_[20 * i + k];

      k = 12;
    }

    // horizontal
    for (auto j = 0U, k = 0U; j < 2U; ++j) {
      co_yield rational_bezier_points_[20 * i + k];
      k += 4;
      co_yield rational_bezier_points_[20 * i + k];

      co_yield rational_bezier_points_[20 * i + k];
      k += 4;
      co_yield rational_bezier_points_[20 * i + k];

      co_yield rational_bezier_points_[20 * i + k];
      k += 4;
      co_yield rational_bezier_points_[20 * i + k];

      k = 3;
    }

    // inner
    co_yield rational_bezier_points_[20 * i + 4];
    co_yield rational_bezier_points_[20 * i + 5];

    co_yield rational_bezier_points_[20 * i + 6];
    co_yield rational_bezier_points_[20 * i + 7];

    co_yield rational_bezier_points_[20 * i + 8];
    co_yield rational_bezier_points_[20 * i + 9];

    co_yield rational_bezier_points_[20 * i + 10];
    co_yield rational_bezier_points_[20 * i + 11];

    co_yield rational_bezier_points_[20 * i + 13];
    co_yield rational_bezier_points_[20 * i + 18];

    co_yield rational_bezier_points_[20 * i + 14];
    co_yield rational_bezier_points_[20 * i + 19];

    co_yield rational_bezier_points_[20 * i + 1];
    co_yield rational_bezier_points_[20 * i + 16];

    co_yield rational_bezier_points_[20 * i + 2];
    co_yield rational_bezier_points_[20 * i + 17];
  }
}

const std::vector<eray::math::Vec3f>& FillInSurface::rational_bezier_points() {
  update();
  return rational_bezier_points_;
}

void FillInSurface::update() {
  if (!points_dirty_) {
    return;
  }
  points_dirty_ = false;

  using BoundaryPoints4 = std::array<eray::math::Vec3f, 4>;
  using BoundaryPoints7 = std::array<eray::math::Vec3f, 7>;

  auto get_boundary_points = [&](const std::array<Boundary, 2>& handles) {
    std::array<BoundaryPoints4, 2> points;

    for (auto b = 0U; const auto& boundary : handles) {
      for (auto i = 0U; const auto& p_h : boundary) {
        if (auto opt = scene().arena<SceneObject>().get_obj(p_h)) {
          const auto& obj = **opt;
          points[b][i++]  = obj.transform.pos();
        } else {
          eray::util::Logger::warn("Could not update the bezier points in fill in surface. ");
          return points;
        }
      }
      ++b;
    }

    return points;
  };

  auto subdivide_bezier = [&](const BoundaryPoints4& points4) {
    BoundaryPoints7 points7;
    auto t = 0.5F;

    // De Casteljau
    points7[0] = points4[0];
    points7[6] = points4[3];

    points7[1] = (1 - t) * points4[0] + t * points4[1];
    points7[5] = (1 - t) * points4[2] + t * points4[3];

    auto p1 = points7[1];
    auto p2 = (1 - t) * points4[1] + t * points4[2];
    auto p3 = points7[5];

    points7[2] = (1 - t) * p1 + t * p2;
    points7[4] = (1 - t) * p2 + t * p3;

    points7[3] = (1 - t) * points7[2] + t * points7[4];

    return points7;
  };

  //
  // The hole:
  //
  //                 0=6
  //                1/ \5
  //   Surface 0   2/ 0 \4    Surface 2
  //              3/\   /\3
  //             4/ 1\ / 2\2
  //            5/____|____\1
  //          6=0 1 2 3 4 5 6=0
  //
  //              Surface 1
  //
  // Gregory patches:
  //
  //            | Surface 2 |
  //         ___|___________|___
  //            |  ___      |
  //            | | 0 |     |
  //  Surface 0 | |___|___  | Surface 2
  //            | | 1 | 2 | |
  //            | |___|___| |
  //         ___|___________|___
  //            | Surface 1 |
  //            |           |
  //
  // Gregory patch:
  //
  //          0----1----2----3
  //          |    |    |    |
  //          |    16  17    |
  //          |              |
  //          4----5    6----7
  //          |              |
  //          8----9   10---11
  //          |              |
  //          |    18  19    |
  //          |    |    |    |
  //          12---13--14---15
  //

  // Boundary bezier subdivided bezier points
  std::array<std::array<BoundaryPoints7, 2>, 3> bezier_points7;  // [surface][row][bezier_point]
  for (auto i = 0U; const auto& neighbor : neighbors_) {
    auto points          = get_boundary_points(neighbor.boundaries);
    bezier_points7[i][0] = subdivide_bezier(points[0]);
    bezier_points7[i][1] = subdivide_bezier(points[1]);
    ++i;
  }

  // Inner points
  std::array<std::array<eray::math::Vec3f, 4>, 3> inner_points;  // [surface][point]
  {
    std::array<eray::math::Vec3f, 3> q;
    for (auto i = 0U; i < kNeighbors; ++i) {
      inner_points[i][3] = bezier_points7[i][0][3];
      inner_points[i][2] = (3.F * bezier_points7[i][0][3] - bezier_points7[i][1][3]) / 2.F;

      q[i] = (3.F * inner_points[i][2] - inner_points[i][3]) / 2.F;
    }
    auto mid = (q[0] + q[1] + q[2]) / 3.F;

    for (auto i = 0U; i < kNeighbors; ++i) {
      inner_points[i][1] = (2.F * q[i] + mid) / 3.F;
      inner_points[i][0] = mid;
    }
  }

  rational_bezier_points_.resize(kNeighbors * 20);
  auto& p = rational_bezier_points_;

  // Surface 0
  for (auto i = 0U, j = 13U; i < 2; ++i) {
    p[20 * i + 0]  = bezier_points7[0][0][3 * i + 0];
    p[20 * i + 4]  = bezier_points7[0][0][3 * i + 1];
    p[20 * i + 8]  = bezier_points7[0][0][3 * i + 2];
    p[20 * i + 12] = bezier_points7[0][0][3 * i + 3];

    p[20 * i + 5] =
        bezier_points7[0][0][3 * i + 1] + (bezier_points7[0][0][3 * i + 1] - bezier_points7[0][1][3 * i + 1]) / 2.F;
    p[20 * i + 9] =
        bezier_points7[0][0][3 * i + 2] + (bezier_points7[0][0][3 * i + 2] - bezier_points7[0][1][3 * i + 2]) / 2.F;

    p[20 * i + j++] = inner_points[0][2];
    p[20 * i + j]   = inner_points[0][1];

    j = 1U;
  }

  // Surface 1
  for (auto i = 1U, j = 7U; i < 3; ++i) {
    p[20 * i + 12] = bezier_points7[1][0][3 * (i - 1) + 0];
    p[20 * i + 13] = bezier_points7[1][0][3 * (i - 1) + 1];
    p[20 * i + 14] = bezier_points7[1][0][3 * (i - 1) + 2];
    p[20 * i + 15] = bezier_points7[1][0][3 * (i - 1) + 3];

    p[20 * i + 18] = bezier_points7[1][0][3 * (i - 1) + 1] +
                     (bezier_points7[1][0][3 * (i - 1) + 1] - bezier_points7[1][1][3 * (i - 1) + 1]) / 2.F;
    p[20 * i + 19] = bezier_points7[1][0][3 * (i - 1) + 2] +
                     (bezier_points7[1][0][3 * (i - 1) + 2] - bezier_points7[1][1][3 * (i - 1) + 2]) / 2.F;

    p[20 * i + j] = inner_points[1][1];
    j += 4;
    p[20 * i + j] = inner_points[1][2];

    j = 4U;
  }
  p[20 + 3] = inner_points[1][0];

  // Surface 2
  p[0] = bezier_points7[2][0][6];
  p[1] = bezier_points7[2][0][5];
  p[2] = bezier_points7[2][0][4];
  p[3] = bezier_points7[2][0][3];

  p[16] = bezier_points7[2][0][5] + (bezier_points7[2][0][5] - bezier_points7[2][1][5]) / 2.F;
  p[17] = bezier_points7[2][0][4] + (bezier_points7[2][0][4] - bezier_points7[2][1][4]) / 2.F;

  p[7]  = inner_points[2][2];
  p[11] = inner_points[2][1];
  p[15] = inner_points[2][0];

  p[2 * 20 + 3]  = bezier_points7[2][0][3];
  p[2 * 20 + 7]  = bezier_points7[2][0][2];
  p[2 * 20 + 11] = bezier_points7[2][0][1];
  p[2 * 20 + 15] = bezier_points7[2][0][0];

  p[2 * 20 + 6]  = bezier_points7[2][0][2] + (bezier_points7[2][0][2] - bezier_points7[2][1][2]) / 2.F;
  p[2 * 20 + 10] = bezier_points7[2][0][1] + (bezier_points7[2][0][1] - bezier_points7[2][1][1]) / 2.F;

  p[2 * 20 + 0] = inner_points[2][0];
  p[2 * 20 + 1] = inner_points[2][1];
  p[2 * 20 + 2] = inner_points[2][2];

  // Add inner tangents to patches
  auto mix_dir = [&](const eray::math::Vec3f& v1, const eray::math::Vec3f& v2) {
    return 2.F / 3.F * v1 + 1.F / 3.F * v2;
  };

  // Patch 0
  auto v1 = p[8] - p[12];
  auto v2 = p[11] - p[15];
  p[18]   = p[13] + mix_dir(v1, v2);
  p[19]   = p[14] + mix_dir(v2, v1);

  v1    = p[14] - p[15];
  v2    = p[2] - p[3];
  p[10] = p[11] + mix_dir(v1, v2);
  p[6]  = p[7] + mix_dir(v2, v1);

  // Patch 1
  v1         = p[20 + 4] - p[20 + 0];
  v2         = p[20 + 7] - p[20 + 3];
  p[20 + 16] = p[20 + 1] + mix_dir(v1, v2);
  p[20 + 17] = p[20 + 2] + mix_dir(v2, v1);

  v1         = p[20 + 2] - p[20 + 3];
  v2         = p[20 + 14] - p[20 + 15];
  p[20 + 6]  = p[20 + 7] + mix_dir(v1, v2);
  p[20 + 10] = p[20 + 11] + mix_dir(v2, v1);

  // Patch 2
  v1            = p[2 * 20 + 13] - p[2 * 20 + 12];
  v2            = p[2 * 20 + 1] - p[2 * 20 + 0];
  p[2 * 20 + 9] = p[2 * 20 + 8] + mix_dir(v1, v2);
  p[2 * 20 + 5] = p[2 * 20 + 4] + mix_dir(v2, v1);

  v1             = p[2 * 20 + 4] - p[2 * 20 + 0];
  v2             = p[2 * 20 + 7] - p[2 * 20 + 3];
  p[2 * 20 + 16] = p[2 * 20 + 1] + mix_dir(v1, v2);
  p[2 * 20 + 17] = p[2 * 20 + 2] + mix_dir(v2, v1);
}

void FillInSurface::on_delete() {
  for (auto& n : neighbors_) {
    if (auto opt = scene().arena<PatchSurface>().get_obj(n.handle)) {
      auto& obj = **opt;
      obj.fill_in_surfaces_.erase(handle());
    }

    for (const auto& b : n.boundaries) {
      for (const auto h : b) {
        if (auto opt = scene().arena<SceneObject>().get_obj(h)) {
          auto& obj = **opt;
          obj.fill_in_surfaces_.erase(handle());
        }
      }
    }
  }

  scene().renderer().push_object_rs_cmd(
      FillInSurfaceRSCommand(handle(), FillInSurfaceRSCommand::Internal::DeleteObject()));
}

void FillInSurface::set_tess_level(int tesselation) {
  // fix tesselation
  auto st     = static_cast<int>(std::sqrt(tesselation));
  tesselation = st * st;

  tess_level_ = tesselation;
  scene().renderer().push_object_rs_cmd(
      FillInSurfaceRSCommand(handle(), FillInSurfaceRSCommand::Internal::UpdateControlPoints()));
}

}  // namespace mini
