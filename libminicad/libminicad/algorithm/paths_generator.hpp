#pragma once

#include <filesystem>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <vector>

namespace mini {

struct MillingDesc {
  eray::math::Vec3f center = eray::math::Vec3f{0.F, 0.F, 0.F};
  float width              = 15.F;
  float height             = 15.F;
};

struct HeightMap {
  static constexpr uint32_t kHeightMapSize = 2048;

  std::vector<float> height_map;
  TextureHandle height_map_handle;
  uint32_t width  = kHeightMapSize;
  uint32_t height = kHeightMapSize;

  static std::optional<HeightMap> load_from_file(Scene& scene, const std::filesystem::path& filename);
  static HeightMap create(Scene& scene, std::vector<PatchSurfaceHandle>& handles,
                          const MillingDesc& desc = MillingDesc{
                              .center = eray::math::Vec3f{0.F, 0.F, 0.F},
                              .width  = 15.F,
                              .height = 15.F,
                          });
  bool save_to_file(const std::filesystem::path& filename);
};

}  // namespace mini
