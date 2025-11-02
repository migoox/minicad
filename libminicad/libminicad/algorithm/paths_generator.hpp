#pragma once

#include <filesystem>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <vector>

namespace mini {

struct WorkpieceDesc {
  float width     = 15.F;
  float height    = 15.F;
  float depth     = 5.F;
  float max_depth = 2.8F;
};

struct HeightMap {
  static constexpr uint32_t kHeightMapSize = 2048;

  std::vector<float> height_map;
  TextureHandle height_map_handle;
  uint32_t width  = kHeightMapSize;
  uint32_t height = kHeightMapSize;

  static std::optional<HeightMap> load_from_file(Scene& scene, const std::filesystem::path& filename);
  static HeightMap create(Scene& scene, std::vector<PatchSurfaceHandle>& handles,
                          const WorkpieceDesc& desc = WorkpieceDesc{});
  bool save_to_file(const std::filesystem::path& filename);
};

struct RoughMillingSolver {
  std::vector<eray::math::Vec3f> points;

  /**
   * @brief Uses spherical milling tool.
   *
   * @param height_map
   * @param desc
   * @param radius in centimeters
   */
  static std::optional<RoughMillingSolver> solve(HeightMap& height_map, const WorkpieceDesc& desc = WorkpieceDesc{},
                                                 float diameter = 1.6F);
};

struct GCodeSerializer {
  static bool write_to_file(const std::vector<eray::math::Vec3f>& points, const std::filesystem::path& filename,
                            float y_offset = 2.2F);
};

}  // namespace mini
