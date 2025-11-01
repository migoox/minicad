#include <iostream>
#include <libminicad/algorithm/paths_generator.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>

#include "liberay/res/image.hpp"
#include "liberay/util/logger.hpp"

namespace mini {

HeightMap HeightMap::create(Scene& scene, std::vector<PatchSurfaceHandle>& handles, const MillingDesc& desc) {
  // Stage 1: Create the height map
  auto height_map = std::vector<float>();
  height_map.resize(kHeightMapSize * kHeightMapSize, desc.center.y);

  auto half_width  = desc.width / 2.F;
  auto half_height = desc.height / 2.F;

  static constexpr uint32_t kSamples      = 4000;
  static constexpr auto kHeightMapSizeFlt = static_cast<float>(kHeightMapSize);

  auto max_h = 0.F;
  for (auto handle : handles) {
    if (auto obj = scene.arena<PatchSurface>().get_obj(handle)) {
      auto& patch_surface = obj.value();
      for (auto i = 0U; i < kSamples; ++i) {
        for (auto j = 0U; j < kSamples; ++j) {
          auto u     = static_cast<float>(i) / static_cast<float>(kSamples);
          auto v     = static_cast<float>(j) / static_cast<float>(kSamples);
          auto val   = patch_surface->evaluate(u, v);
          bool valid = val.x > -half_width && val.x < half_width && val.z > -half_height && val.z < half_height;
          if (!valid) {
            continue;
          }

          //  x,z -> [0, kHeightMapSize)
          float x_f = (val.x + half_width) / desc.width * kHeightMapSizeFlt;
          float z_f = (val.z + half_height) / desc.height * kHeightMapSizeFlt;

          auto h_ind = static_cast<size_t>(z_f) * kHeightMapSize + static_cast<size_t>(x_f);

          if (h_ind < kHeightMapSize * kHeightMapSize) {
            height_map[h_ind] = std::max(height_map[h_ind], val.y);
            max_h             = std::max(max_h, val.y);
          }
        }
      }
    }
  }

  // height map texture
  auto temp_texture = std::vector<uint32_t>();
  temp_texture.resize(kHeightMapSize * kHeightMapSize);
  if (max_h > 0.F) {
    for (auto i = 0U; i < kHeightMapSize; ++i) {
      for (auto j = 0U; j < kHeightMapSize; ++j) {
        auto v                               = height_map[i * kHeightMapSize + j] / max_h;
        temp_texture[i * kHeightMapSize + j] = eray::res::Color::from_rgb_norm(v, v, v);
      }
    }
  }
  auto txt_handle = scene.renderer().upload_texture(temp_texture, kHeightMapSize, kHeightMapSize);

  return HeightMap{
      .height_map        = std::move(height_map),
      .height_map_handle = txt_handle,
      .width             = kHeightMapSize,
      .height            = kHeightMapSize,
  };
}

bool HeightMap::save_to_file(const std::filesystem::path& filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    eray::util::Logger::info("Failed to open the file with path {}", filename.string());
    return false;
  }

  file << width << " " << height << "\n";
  for (float h : height_map) {
    file << h << "\n";
  }

  return true;
}

std::optional<HeightMap> HeightMap::load_from_file(Scene& scene, const std::filesystem::path& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    eray::util::Logger::info("Failed to load file with path {}", filename.string());
    return std::nullopt;
  }

  uint32_t width  = 0;
  uint32_t height = 0;
  file >> width >> height;
  if (height > kHeightMapSize || width > kHeightMapSize) {
    eray::util::Logger::err("Height map dimensions exceed the max dimensions");
    return std::nullopt;
  }

  auto height_map = std::vector<float>();
  height_map.reserve(kHeightMapSize * kHeightMapSize);
  float h     = 0.F;
  float max_h = 0.F;
  while (file >> h) {
    height_map.push_back(h);
    max_h = std::max(h, max_h);
  }

  auto temp_texture = std::vector<uint32_t>();
  temp_texture.resize(kHeightMapSize * kHeightMapSize);
  if (max_h > 0.F) {
    for (auto i = 0U; i < kHeightMapSize; ++i) {
      for (auto j = 0U; j < kHeightMapSize; ++j) {
        auto v                               = height_map[i * kHeightMapSize + j] / max_h;
        temp_texture[i * kHeightMapSize + j] = eray::res::Color::from_rgb_norm(v, v, v);
      }
    }
  }
  auto txt_handle = scene.renderer().upload_texture(temp_texture, kHeightMapSize, kHeightMapSize);

  if (height_map.size() != width * height) {
    eray::util::Logger::warn("Expected {} but read {}", (width * height), height_map.size());
  }

  return HeightMap{
      .height_map        = std::move(height_map),
      .height_map_handle = txt_handle,
      .width             = width,
      .height            = height,
  };
}

}  // namespace mini
