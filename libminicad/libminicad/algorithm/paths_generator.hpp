#pragma once

#include <filesystem>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <vector>

#include "libminicad/scene/handles.hpp"

namespace mini {

struct WorkpieceDesc {
  float width     = 15.F;
  float height    = 15.F;
  float depth     = 5.F;
  float max_depth = 3.4F;
};

struct HeightMap {
  static constexpr uint32_t kHeightMapSize = 1500;  // accuracy: 1.5mm

  std::vector<float> height_map;
  std::vector<eray::math::Vec3f> normal_map;
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

struct FlatMillingSolver {
  std::vector<eray::math::Vec3f> points;
  TextureHandle border_handle;

  /**
   * @brief Uses flat milling tool.
   *
   * @param height_map
   * @param desc
   * @param radius in centimeters
   */
  static std::optional<FlatMillingSolver> solve(Scene& scene, HeightMap& height_map,
                                                const WorkpieceDesc& desc = WorkpieceDesc{}, float diameter = 1.0F);
};

struct DetailedMillingSolver {
  std::vector<eray::math::Vec3f> points;

  /**
   * @brief Uses flat milling tool.
   *
   * @param height_map
   * @param desc
   * @param radius in centimeters
   */
  static std::optional<DetailedMillingSolver> solve(Scene& scene, const std::vector<PatchSurfaceHandle>& patch_handles,
                                                    const WorkpieceDesc& desc = WorkpieceDesc{}, float diameter = 0.8F);
};

struct GCodeSerializer {
  static bool write_to_file(const std::vector<eray::math::Vec3f>& points, const std::filesystem::path& filename,
                            const WorkpieceDesc& desc = WorkpieceDesc{});
};

}  // namespace mini
