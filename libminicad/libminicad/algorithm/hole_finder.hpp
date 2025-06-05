#pragma once

#include <cstdint>
#include <expected>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini {

class BezierHole3Finder {
 public:
  struct Edge {
    static Edge create(uint32_t u, uint32_t v) {
      if (u > v) {
        std::swap(u, v);
      }
      return Edge{.u = u, .v = v};
    }

    uint32_t u;
    uint32_t v;

    bool operator==(const Edge& other) const { return u == other.u && v == other.v; }
  };

  struct PatchEdgeInfo {
    static PatchEdgeInfo create(uint32_t patch_surface_id, uint32_t patch_id, uint32_t u_idx, uint32_t v_idx) {
      if (u_idx > v_idx) {
        std::swap(u_idx, v_idx);
      }
      return PatchEdgeInfo{.patch_surface_id = patch_surface_id, .patch_id = patch_id, .u_idx = u_idx, .v_idx = v_idx};
    }

    uint32_t patch_surface_id;
    uint32_t patch_id;
    uint32_t u_idx;
    uint32_t v_idx;

    bool is_same_patch(const PatchEdgeInfo& other) const {
      return patch_surface_id == other.patch_surface_id && patch_id == other.patch_id;
    }
  };

  using BezierHole = std::array<PatchEdgeInfo, 3>;

  enum class FinderError : uint8_t { NotABezierPatchSurface = 0, HandleNotValid = 1 };

  static std::expected<std::vector<BezierHole>, FinderError> find_holes(
      const Scene& scene, const std::vector<PatchSurfaceHandle>& bezier_surface_handles);
};

}  // namespace mini

namespace std {
template <>
struct hash<mini::BezierHole3Finder::Edge> {
  size_t operator()(const mini::BezierHole3Finder::Edge& edge) const noexcept {
    size_t h1 = edge.u;
    size_t h2 = edge.v;
    eray::util::hash_combine(h1, h2);
    return h1;
  }
};

template <>
struct hash<mini::BezierHole3Finder::PatchEdgeInfo> {
  size_t operator()(const mini::BezierHole3Finder::PatchEdgeInfo& pinfo) const noexcept {
    size_t h1 = pinfo.patch_id;
    size_t h2 = pinfo.patch_surface_id;
    eray::util::hash_combine(h1, h2);
    return h1;
  }
};

}  // namespace std
