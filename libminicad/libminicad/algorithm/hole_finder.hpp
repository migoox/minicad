#pragma once

#include <cstdint>
#include <expected>
#include <liberay/math/vec.hpp>
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

  struct PatchEdgeInternalInfo {
    enum class BoundaryDirection : uint8_t { Up, Down, Left, Right };

    static PatchEdgeInternalInfo create(uint32_t patch_surface_id, eray::math::Vec2u patch_coords,
                                        BoundaryDirection boundary_dir) {
      return PatchEdgeInternalInfo{
          .patch_surface_id = patch_surface_id,
          .patch_coords     = patch_coords,
          .boundary_dir     = boundary_dir,
      };
    }

    uint32_t patch_surface_id;
    eray::math::Vec2u patch_coords;
    BoundaryDirection boundary_dir;

    bool is_same_patch(const PatchEdgeInternalInfo& other) const {
      return patch_surface_id == other.patch_surface_id && patch_coords.x == other.patch_coords.x &&
             patch_coords.y == other.patch_coords.y;
    }
  };

  struct PatchEdgeInfo {
    enum class CreationError : uint8_t {
      InvalidPatchSurfaceId = 0,
      InvalidPatchIdx       = 1,
    };

    static std::expected<PatchEdgeInfo, CreationError> create(const Scene& scene, const PatchEdgeInternalInfo& pinfo);

    PatchSurfaceHandle patch_surface_handle_;
    std::array<std::array<SceneObjectHandle, 4>, 2> boundary_;  // row-major, rows = 4, columns = 2
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
    auto h1 = static_cast<size_t>(edge.u);
    auto h2 = static_cast<size_t>(edge.v);
    eray::util::hash_combine(h1, h2);
    return h1;
  }
};

template <>
struct hash<mini::BezierHole3Finder::PatchEdgeInternalInfo> {
  size_t operator()(const mini::BezierHole3Finder::PatchEdgeInternalInfo& pinfo) const noexcept {
    auto h1 = static_cast<size_t>(pinfo.patch_coords.x);
    auto h2 = static_cast<size_t>(pinfo.patch_coords.y);
    auto h3 = static_cast<size_t>(pinfo.patch_surface_id);
    auto h4 = static_cast<size_t>(pinfo.boundary_dir);
    eray::util::hash_combine(h1, h2);
    eray::util::hash_combine(h1, h3);
    eray::util::hash_combine(h1, h4);
    return h1;
  }
};

}  // namespace std
