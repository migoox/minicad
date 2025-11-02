#include <iostream>
#include <libminicad/algorithm/paths_generator.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>

#include "liberay/math/vec_fwd.hpp"
#include "liberay/res/image.hpp"
#include "liberay/util/logger.hpp"

namespace mini {

namespace math = eray::math;

HeightMap HeightMap::create(Scene& scene, std::vector<PatchSurfaceHandle>& handles, const WorkpieceDesc& desc) {
  // Stage 1: Create the height map
  auto height_map = std::vector<float>();
  height_map.resize(kHeightMapSize * kHeightMapSize, 0.F);

  const auto half_width  = desc.width / 2.F;
  const auto half_height = desc.height / 2.F;

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

std::optional<RoughMillingSolver> RoughMillingSolver::solve(HeightMap& height_map, const WorkpieceDesc& desc,
                                                            const float diameter) {
  const auto radius              = diameter / 2.F;
  const auto half_width          = desc.width / 2.F;
  const auto half_height         = desc.height / 2.F;
  const auto safety_offset       = radius;
  const auto safety_depth_offset = 0.4F;
  const auto layer1_y            = desc.max_depth - radius;
  const auto layer2_y            = desc.max_depth - diameter;

  auto points = std::vector<math::Vec3f>();
  points.emplace_back(0.F, safety_offset + desc.max_depth, 0.F);
  points.emplace_back(safety_offset + half_width, safety_offset + desc.max_depth, safety_offset + half_height);
  points.emplace_back(safety_offset + half_width, layer1_y, safety_offset + half_height);

  // Layer 1
  uint32_t i        = 0;
  float curr_height = half_height;
  while (curr_height > -half_height) {
    const float start_x = i % 2 == 0 ? safety_offset + half_width : -safety_offset - half_width;
    const float end_x   = i % 2 == 0 ? -safety_offset - half_width : safety_offset + half_width;

    auto ind_y = static_cast<size_t>((curr_height / desc.height + 0.5F) * static_cast<float>(height_map.height));

    points.emplace_back(start_x, layer1_y, curr_height);
    if (ind_y < height_map.width) {
      bool collision = false;
      for (auto j = 0U; j < height_map.width; ++j) {
        const auto jj = i % 2 == 0 ? height_map.width - j : j;
        const auto h  = height_map.height_map[jj + height_map.width * ind_y] + radius + safety_depth_offset;
        const auto x  = (static_cast<float>(jj) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
        if (h > layer1_y) {
          points.emplace_back(x, h, curr_height);
          collision = true;
        } else if (collision) {
          points.emplace_back(x, layer1_y, curr_height);
          collision = false;
        }
      }
    }
    points.emplace_back(end_x, layer1_y, curr_height);

    ++i;
    curr_height = half_height - radius * static_cast<float>(i);
  }

  int odd = 0;
  if (i % 2 == 0) {
    points.emplace_back(safety_offset + half_width, layer1_y, curr_height);
    points.emplace_back(-safety_offset - half_width, layer1_y, curr_height);
    odd = 1;
  } else {
    points.emplace_back(-safety_offset - half_width, layer1_y, curr_height);
    points.emplace_back(safety_offset + half_width, layer1_y, curr_height);
    odd = 0;
  }

  // Layer 2
  i                  = 0;
  float start_height = curr_height;
  while (curr_height < half_height) {
    const float start_x = i % 2 == odd ? safety_offset + half_width : -safety_offset - half_width;
    const float end_x   = i % 2 == odd ? -safety_offset - half_width : safety_offset + half_width;

    auto ind_y = static_cast<size_t>((curr_height / desc.height + 0.5F) * static_cast<float>(height_map.height));

    points.emplace_back(start_x, layer2_y, curr_height);
    if (ind_y < height_map.width) {
      bool collision = false;
      for (auto j = 0U; j < height_map.width; ++j) {
        const auto jj = i % 2 == 0 ? height_map.width - j : j;
        const auto h  = height_map.height_map[jj + height_map.width * ind_y] + radius + safety_depth_offset;
        const auto x  = (static_cast<float>(jj) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
        if (h > layer2_y) {
          points.emplace_back(x, h, curr_height);
          collision = true;
        } else if (collision) {
          points.emplace_back(x, layer2_y, curr_height);
          collision = false;
        }
      }
    }
    points.emplace_back(end_x, layer2_y, curr_height);

    ++i;
    curr_height = start_height + radius * static_cast<float>(i);
  }

  if (i % 2 == 0) {
    points.emplace_back(safety_offset + half_width, layer2_y, curr_height);
    points.emplace_back(-safety_offset - half_width, layer2_y, curr_height);
    points.emplace_back(-safety_offset - half_width, layer2_y, curr_height + safety_offset);
    points.emplace_back(-safety_offset - half_width, safety_offset + desc.max_depth, curr_height + safety_offset);
  } else {
    points.emplace_back(-safety_offset - half_width, layer2_y, curr_height);
    points.emplace_back(safety_offset + half_width, layer2_y, curr_height);
    points.emplace_back(safety_offset + half_width, layer2_y, curr_height + safety_offset);
    points.emplace_back(safety_offset + half_width, safety_offset + desc.max_depth, curr_height + safety_offset);
  }
  points.emplace_back(0.F, safety_offset + desc.max_depth, 0.F);

  return RoughMillingSolver{.points = points};
}

bool GCodeSerializer::write_to_file(const std::vector<eray::math::Vec3f>& points, const std::filesystem::path& filename,
                                    float y_offset) {
  std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
  if (!ofs.is_open()) {
    eray::util::Logger::info("Could not open a file with path {}", filename.string());
    return false;
  }

  const auto* end_line = "\n";

  int n = 1;

  auto format_coord = [](float v) -> std::string {
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss << std::setprecision(3) << v;
    return ss.str();
  };

  for (const auto& p : points) {
    std::string xs = format_coord(p.x * 10.F);
    std::string ys = format_coord(p.z * 10.F);
    std::string zs = format_coord((p.y + y_offset) * 10.F);

    ofs << 'N' << n++ << "G01" << 'X' << xs << 'Y' << ys << 'Z' << zs << end_line;
  }

  ofs.flush();
  ofs.close();
  return true;
}

}  // namespace mini
