#include <expected>
#include <liberay/util/container_extensions.hpp>
#include <liberay/util/logger.hpp>
#include <libminicad/algorithm/hole_finder.hpp>
#include <libminicad/scene/handles.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <unordered_map>

namespace mini {

std::expected<std::vector<BezierHole3Finder::BezierHole>, BezierHole3Finder::FinderError> BezierHole3Finder::find_holes(
    const Scene& scene, const std::vector<PatchSurfaceHandle>& bezier_surface_handles) {
  using EdgeIterator   = std::unordered_map<Edge, std::vector<PatchEdgeInternalInfo>>::iterator;
  auto edges           = std::unordered_map<Edge, std::vector<PatchEdgeInternalInfo>>();
  auto adjacency_lists = std::unordered_map<uint32_t, std::unordered_set<uint32_t>>();
  auto add_edge        = [&edges, &adjacency_lists](const Edge& edge, const PatchEdgeInternalInfo& pinfo) {
    auto [u_it, _1] = adjacency_lists.try_emplace(edge.u);
    auto [v_it, _2] = adjacency_lists.try_emplace(edge.v);

    u_it->second.emplace(edge.v);
    v_it->second.emplace(edge.u);

    auto [e_it, _3] = edges.try_emplace(edge);
    e_it->second.push_back(pinfo);
  };
  auto remove_edge_and_go_next = [&edges, &adjacency_lists](EdgeIterator& edge_it) {
    adjacency_lists.at(edge_it->first.v).erase(edge_it->first.u);
    adjacency_lists.at(edge_it->first.u).erase(edge_it->first.v);
    edge_it = edges.erase(edge_it);
  };

  // Create a graph from all patches
  for (const auto& bsh : bezier_surface_handles) {
    if (auto opt = scene.arena<PatchSurface>().get_obj(bsh)) {
      const auto& obj = **opt;
      if (!obj.has_type<BezierPatches>()) {
        eray::util::Logger::err("Bezier Hole3 finder received a patch that is not a bezier patch");
        return std::unexpected(FinderError::NotABezierPatchSurface);
      }

      auto patches_control_point_handles = obj.patches_control_point_handles();

      for (const auto& full_patch : patches_control_point_handles) {
        auto [patch, patch_coords] = full_patch;

        auto ul_cp = patch[0][0].obj_id;
        auto ur_cp = patch[0][3].obj_id;
        auto br_cp = patch[3][3].obj_id;
        auto bl_cp = patch[3][0].obj_id;

        if (ul_cp != ur_cp) {
          add_edge(Edge::create(ul_cp, ur_cp),  //
                   PatchEdgeInternalInfo::create(obj.handle().obj_id, patch_coords,
                                                 PatchEdgeInternalInfo::BoundaryDirection::Up)  //
          );
        }
        if (ur_cp != br_cp) {
          add_edge(Edge::create(ur_cp, br_cp),  //
                   PatchEdgeInternalInfo::create(obj.handle().obj_id, patch_coords,
                                                 PatchEdgeInternalInfo::BoundaryDirection::Right)  //
          );
        }
        if (br_cp != bl_cp) {
          add_edge(Edge::create(br_cp, bl_cp),  //
                   PatchEdgeInternalInfo::create(obj.handle().obj_id, patch_coords,
                                                 PatchEdgeInternalInfo::BoundaryDirection::Down)  //
          );
        }
        if (bl_cp != ul_cp) {
          add_edge(Edge::create(bl_cp, ul_cp),  //
                   PatchEdgeInternalInfo::create(obj.handle().obj_id, patch_coords,
                                                 PatchEdgeInternalInfo::BoundaryDirection::Left)  //
          );
        }
      }
    } else {
      eray::util::Logger::err("Bezier Hole3 finder received a patch that does not exist");
      return std::unexpected(FinderError::HandleNotValid);
    }
  }

  auto holes = std::vector<BezierHole>();
  for (auto uv_it = edges.begin(); uv_it != edges.end();) {
    const auto& [uv, patches_uv] = *uv_it;

    const auto& u_neighbors = adjacency_lists.at(uv.u);
    const auto& v_neighbors = adjacency_lists.at(uv.v);

    for (auto w : u_neighbors) {
      if (v_neighbors.contains(w)) {
        // Triangles found
        auto uw                = Edge::create(uv.u, w);
        auto vw                = Edge::create(uv.v, w);
        const auto& patches_uw = edges.at(uw);
        const auto& patches_vw = edges.at(vw);

        // Note: The typical case is when there is only one patch per edge so these loops shouldn't hurt much
        // as typically the code simplifies to only one if statement in the 3rd loop. The for
        // loops are here for the generality sake (multigraphs).
        for (const auto& p_uv : patches_uv) {
          for (const auto& p_uw : patches_uw) {
            for (const auto& p_vw : patches_vw) {
              if (!p_uv.is_same_patch(p_uw) && !p_uw.is_same_patch(p_vw) && !p_uv.is_same_patch(p_vw)) {
                holes.emplace_back(BezierHole{
                    *PatchEdgeInfo::create(scene, p_uv),  //
                    *PatchEdgeInfo::create(scene, p_uw),  //
                    *PatchEdgeInfo::create(scene, p_vw)   //
                });
              }
            }
          }
        }
      }
    }

    remove_edge_and_go_next(uv_it);  // all possible triangles for that edge are already tested
  }

  return holes;
}

std::expected<BezierHole3Finder::PatchEdgeInfo, BezierHole3Finder::PatchEdgeInfo::CreationError>
BezierHole3Finder::PatchEdgeInfo::create(const Scene& scene, const PatchEdgeInternalInfo& pinfo) {
  if (auto opt = scene.arena<PatchSurface>().get_obj_by_id(pinfo.patch_surface_id)) {
    const auto& obj = **opt;
    if (const auto& patch_opt = obj.patch_control_point_handles(pinfo.patch_coords)) {
      const auto& patch = *patch_opt;

      auto boundary = eray::util::make_filled_array<std::array<PointObjectHandle, PatchSurface::kPatchSize>, 2>(
          eray::util::make_filled_array<PointObjectHandle, PatchSurface::kPatchSize>(PointObjectHandle(0, 0, 0))  //
      );

      if (pinfo.boundary_dir == PatchEdgeInternalInfo::BoundaryDirection::Up) {
        for (auto i = 0U; i < 2; ++i) {
          for (auto j = 0U; j < PatchSurface::kPatchSize; ++j) {
            boundary[i][j] = patch[i][j];
          }
        }
      }
      if (pinfo.boundary_dir == PatchEdgeInternalInfo::BoundaryDirection::Right) {
        for (auto i = 0U; i < 2; ++i) {
          for (auto j = 0U; j < PatchSurface::kPatchSize; ++j) {
            boundary[i][j] = patch[PatchSurface::kPatchSize - j - 1][PatchSurface::kPatchSize - i - 1];
          }
        }
      }
      if (pinfo.boundary_dir == PatchEdgeInternalInfo::BoundaryDirection::Down) {
        for (auto i = 0U; i < 2; ++i) {
          for (auto j = 0U; j < PatchSurface::kPatchSize; ++j) {
            boundary[i][j] = patch[PatchSurface::kPatchSize - i - 1][j];
          }
        }
      }
      if (pinfo.boundary_dir == PatchEdgeInternalInfo::BoundaryDirection::Left) {
        for (auto i = 0U; i < 2; ++i) {
          for (auto j = 0U; j < PatchSurface::kPatchSize; ++j) {
            boundary[i][j] = patch[PatchSurface::kPatchSize - j - 1][i];
          }
        }
      }

      return PatchEdgeInfo{
          .patch_surface_handle_ = obj.handle(),
          .boundary_             = boundary,
      };
    }

    eray::util::Logger::err("The patch surface does not contain a patch with the provided index!");
    return std::unexpected(CreationError::InvalidPatchIdx);
  }

  eray::util::Logger::err("The patch surface id is invalid!");
  return std::unexpected(CreationError::InvalidPatchSurfaceId);
}

}  // namespace mini
