#include <expected>
#include <liberay/util/logger.hpp>
#include <libminicad/algorithm/hole_finder.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <unordered_map>

namespace mini {

std::expected<std::vector<BezierHole3Finder::BezierHole>, BezierHole3Finder::FinderError> BezierHole3Finder::find_holes(
    const Scene& scene, const std::vector<PatchSurfaceHandle>& bezier_surface_handles) {
  auto edges           = std::unordered_map<Edge, std::vector<PatchEdgeInfo>>();
  auto adjacency_lists = std::unordered_map<uint32_t, std::unordered_set<uint32_t>>();
  auto add_edge        = [&edges, &adjacency_lists](const Edge& edge, const PatchEdgeInfo& pinfo) {
    auto [u_it, _1] = adjacency_lists.try_emplace(edge.u);
    auto [v_it, _2] = adjacency_lists.try_emplace(edge.v);

    u_it->second.emplace(edge.v);
    v_it->second.emplace(edge.u);

    auto [e_it, _3] = edges.try_emplace(edge);
    e_it->second.push_back(pinfo);
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

      for (auto patch_idx = 0U; const auto& patch : patches_control_point_handles) {
        auto [ul_cp, ul_idx] = patch[0][0];
        auto [ur_cp, ur_idx] = patch[0][3];
        auto [bl_cp, bl_idx] = patch[3][0];
        auto [br_cp, br_idx] = patch[3][3];

        if (ul_cp.obj_id != ur_cp.obj_id) {
          add_edge(Edge::create(ul_cp.obj_id, ur_cp.obj_id),                              //
                   PatchEdgeInfo::create(obj.handle().obj_id, patch_idx, ul_idx, ur_idx)  //
          );
        }
        if (ur_cp.obj_id != bl_cp.obj_id) {
          add_edge(Edge::create(ur_cp.obj_id, bl_cp.obj_id),                              //
                   PatchEdgeInfo::create(obj.handle().obj_id, patch_idx, ur_idx, bl_idx)  //
          );
        }
        if (bl_cp.obj_id != br_cp.obj_id) {
          add_edge(Edge::create(bl_cp.obj_id, br_cp.obj_id),                              //
                   PatchEdgeInfo::create(obj.handle().obj_id, patch_idx, bl_idx, br_idx)  //
          );
        }
        if (br_cp.obj_id != ul_cp.obj_id) {
          add_edge(Edge::create(br_cp.obj_id, ul_cp.obj_id),                              //
                   PatchEdgeInfo::create(obj.handle().obj_id, patch_idx, br_idx, ul_idx)  //
          );
        }

        ++patch_idx;
      }
    } else {
      eray::util::Logger::err("Bezier Hole3 finder received a patch that does not exist");
      return std::unexpected(FinderError::HandleNotValid);
    }
  }

  auto holes = std::vector<BezierHole>();
  for (const auto& [uv, patches_uv] : edges) {
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
        // loops are here for the generality sake.
        for (const auto& p_uv : patches_uv) {
          for (const auto& p_uw : patches_uw) {
            for (const auto& p_vw : patches_vw) {
              if (!p_uv.is_same_patch(p_uw) && p_uw.is_same_patch(p_vw) && p_uv.is_same_patch(p_vw)) {
                holes.emplace_back(BezierHole{p_uv, p_uw, p_vw});
              }
            }
          }
        }
      }
    }
  }

  return holes;
}

}  // namespace mini
