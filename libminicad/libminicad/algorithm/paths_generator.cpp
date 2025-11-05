#include <iostream>
#include <libminicad/algorithm/paths_generator.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "liberay/math/vec.hpp"
#include "liberay/math/vec_fwd.hpp"
#include "liberay/res/image.hpp"
#include "liberay/util/logger.hpp"

namespace mini {

namespace math = eray::math;

HeightMap HeightMap::create(Scene& scene, std::vector<PatchSurfaceHandle>& handles, const WorkpieceDesc& desc) {
  // Stage 1: Create the height map
  auto height_map = std::vector<float>();
  height_map.resize(kHeightMapSize * kHeightMapSize, 0.F);
  auto normal_map = std::vector<math::Vec3f>();
  normal_map.resize(kHeightMapSize * kHeightMapSize, math::Vec3f{0.F, 1.F, 0.F});

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
          auto deriv = patch_surface->evaluate_derivatives(u, v);
          auto norm  = eray::math::cross(deriv.first, deriv.second).normalize();

          bool valid = val.x > -half_width && val.x < half_width && val.z > -half_height && val.z < half_height;
          if (!valid) {
            continue;
          }

          //  x,z -> [0, kHeightMapSize)
          float x_f = (val.x + half_width) / desc.width * kHeightMapSizeFlt;
          float z_f = (val.z + half_height) / desc.height * kHeightMapSizeFlt;

          auto ind = static_cast<size_t>(z_f) * kHeightMapSize + static_cast<size_t>(x_f);

          if (ind < kHeightMapSize * kHeightMapSize) {
            if (height_map[ind] < val.y) {
              height_map[ind] = val.y;
              normal_map[ind] = norm;
            }
            max_h = std::max(max_h, val.y);
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
      .normal_map        = std::move(normal_map),
      .height_map_handle = txt_handle,
      .width             = kHeightMapSize,
      .height            = kHeightMapSize,
  };
}

bool HeightMap::save_to_file(const std::filesystem::path& filename) {
  std::ofstream file(filename);
  file << std::fixed << std::setprecision(6);
  if (!file.is_open()) {
    eray::util::Logger::info("Failed to open the file with path {}", filename.string());
    return false;
  }
  auto sanitize = [](float v) {
    if (!std::isfinite(v)) {
      return 0.0F;
    }
    return v;
  };

  file << width << " " << height << "\n";
  for (auto i = 0U; i < width * height; ++i) {
    file << sanitize(height_map[i]) << " " << sanitize(normal_map[i].x) << " " << sanitize(normal_map[i].y) << " "
         << sanitize(normal_map[i].z) << "\n";
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
  auto normal_map = std::vector<math::Vec3f>();
  normal_map.reserve(kHeightMapSize * kHeightMapSize);
  float max_h = 0.F;
  {
    for (auto i = 0U; i < width * height; ++i) {
      float h = 0.F;
      float x = 0.F;
      float y = 0.F;
      float z = 0.F;
      if (!(file >> h >> x >> y >> z)) {
        eray::util::Logger::err("Unexpected end of file while reading height map data");
        return std::nullopt;
      }
      height_map.push_back(h);
      normal_map.emplace_back(x, y, z);
      max_h = std::max(h, max_h);
    }
  }

  auto temp_texture = std::vector<uint32_t>();
  temp_texture.resize(width * height);
  if (max_h > 0.F) {
    for (auto i = 0U; i < height; ++i) {
      for (auto j = 0U; j < width; ++j) {
        auto v                      = height_map[i * width + j] / max_h;
        temp_texture[i * width + j] = eray::res::Color::from_rgb_norm(v, v, v);
      }
    }
  }
  auto txt_handle = scene.renderer().upload_texture(temp_texture, kHeightMapSize, kHeightMapSize);

  if (height_map.size() != width * height) {
    eray::util::Logger::warn("Expected {} but read {}", (width * height), height_map.size());
  }

  return HeightMap{
      .height_map        = std::move(height_map),
      .normal_map        = std::move(normal_map),
      .height_map_handle = txt_handle,
      .width             = width,
      .height            = height,
  };
}

std::optional<RoughMillingSolver> RoughMillingSolver::solve(HeightMap& height_map, const WorkpieceDesc& desc,
                                                            const float diameter) {
  const auto radius        = diameter / 2.F;
  const auto half_width    = desc.width / 2.F;
  const auto half_height   = desc.height / 2.F;
  const auto safety_offset = radius;
  const auto layer1_y      = desc.max_depth - diameter;
  const auto layer2_y      = desc.max_depth - 2.F * diameter;

  auto points = std::vector<math::Vec3f>();
  points.emplace_back(0.F, safety_offset + desc.max_depth, 0.F);
  points.emplace_back(safety_offset + half_width, safety_offset + desc.max_depth, safety_offset + half_height);
  points.emplace_back(safety_offset + half_width, layer1_y, safety_offset + half_height);

  auto collision_fix = [&](math::Vec3f sphere_center) {
    auto ind_right =
        static_cast<size_t>(((sphere_center.x + radius) / desc.width + 0.5F) * static_cast<float>(height_map.width));
    auto ind_left =
        static_cast<size_t>(((sphere_center.x - radius) / desc.width + 0.5F) * static_cast<float>(height_map.width));

    ind_right = std::clamp<size_t>(ind_right, 0U, height_map.width);
    ind_left  = std::clamp<size_t>(ind_left, 0U, height_map.width);

    auto ind_top =
        static_cast<size_t>(((sphere_center.z + radius) / desc.height + 0.5F) * static_cast<float>(height_map.height));
    auto ind_bottom =
        static_cast<size_t>(((sphere_center.z - radius) / desc.height + 0.5F) * static_cast<float>(height_map.height));

    ind_bottom = std::clamp<size_t>(ind_bottom, 0U, height_map.height);
    ind_top    = std::clamp<size_t>(ind_top, 0U, height_map.height);

    auto max_offset = 0.F;
    for (auto i = ind_left; i < ind_right; ++i) {
      for (auto j = ind_top; j < ind_bottom; ++j) {
        const auto x = (static_cast<float>(i) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
        const auto y = height_map.height_map[j * height_map.width + i];
        const auto z = (static_cast<float>(j) / static_cast<float>(height_map.height) - 0.5F) * desc.height;
        auto hp      = math::Vec3f{x, y, z};

        if (math::dot(hp - sphere_center, hp - sphere_center) < radius * radius) {
          const auto dx = std::abs(hp.x - sphere_center.x);
          const auto dy = std::abs(hp.y - sphere_center.y);
          const auto dh = std::sqrt(radius * radius - dx * dx) - dy;

          max_offset = std::max(dh, max_offset);
        }
      }
    }

    sphere_center.y += max_offset;
    return sphere_center;
  };

  // Layer 1
  uint32_t i        = 0;
  float curr_height = half_height;
  while (curr_height > -half_height) {
    const float start_x = i % 2 == 0 ? safety_offset + half_width : -safety_offset - half_width;
    const float end_x   = i % 2 == 0 ? -safety_offset - half_width : safety_offset + half_width;

    auto ind_y = static_cast<size_t>((curr_height / desc.height + 0.5F) * static_cast<float>(height_map.height));

    points.emplace_back(start_x, layer1_y, curr_height);
    if (ind_y < height_map.width) {
      for (auto j = 0U; j < height_map.width; j += 2) {
        const auto jj  = i % 2 == 0 ? height_map.width - j : j;
        const auto ind = jj + height_map.width * ind_y;

        const auto h = height_map.height_map[ind];
        const auto x = (static_cast<float>(jj) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
        if (h > layer1_y) {
          auto norm_flat = math::Vec3f{height_map.normal_map[ind].x, height_map.normal_map[ind].y, 0.F};
          norm_flat      = norm_flat.normalize();

          auto sphere_center = math::Vec3f{x, h, curr_height} + norm_flat * radius;
          auto p             = collision_fix(sphere_center);
          p.y -= radius;
          p.y = std::max(p.y, layer1_y);
          if (i % 2 == 0 && points.back().x > p.x) {
            points.push_back(p);
          }
          if (i % 2 != 0 && points.back().x < p.x) {
            points.push_back(p);
          }
        }
      }
    }
    points.emplace_back(end_x, layer1_y, curr_height);

    ++i;
    curr_height = half_height - radius * static_cast<float>(i);
  }

  int even = 0;
  if (i % 2 == 0) {
    points.emplace_back(safety_offset + half_width, layer1_y, curr_height);
    points.emplace_back(-safety_offset - half_width, layer1_y, curr_height);
    even = 1;
  } else {
    points.emplace_back(-safety_offset - half_width, layer1_y, curr_height);
    points.emplace_back(safety_offset + half_width, layer1_y, curr_height);
    even = 0;
  }

  // Layer 2
  i                  = 0;
  float start_height = curr_height;
  while (curr_height < half_height) {
    const float start_x = i % 2 == even ? safety_offset + half_width : -safety_offset - half_width;
    const float end_x   = i % 2 == even ? -safety_offset - half_width : safety_offset + half_width;

    auto ind_y = static_cast<size_t>((curr_height / desc.height + 0.5F) * static_cast<float>(height_map.height));

    points.emplace_back(start_x, layer2_y, curr_height);
    if (ind_y < height_map.width) {
      for (auto j = 0U; j < height_map.width; ++j) {
        const auto jj  = i % 2 == 0 ? height_map.width - j : j;
        const auto ind = jj + height_map.width * ind_y;

        const auto h = height_map.height_map[ind];
        const auto x = (static_cast<float>(jj) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
        if (h > layer2_y) {
          auto norm_flat = math::Vec3f{height_map.normal_map[ind].x, height_map.normal_map[ind].y, 0.F};
          norm_flat      = norm_flat.normalize();

          auto sphere_center = math::Vec3f{x, h, curr_height} + norm_flat * radius;
          auto p             = collision_fix(sphere_center);
          p.y -= radius;
          p.y = std::max(p.y, layer2_y);
          if (i % 2 == even && points.back().x > p.x) {
            points.push_back(p);
          }
          if (i % 2 != even && points.back().x < p.x) {
            points.push_back(p);
          }
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
                                    const WorkpieceDesc& desc) {
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
    std::string zs = format_coord((p.y + (desc.depth - desc.max_depth)) * 10.F);

    ofs << 'N' << n++ << "G01" << 'X' << xs << 'Y' << ys << 'Z' << zs << end_line;
  }

  ofs.flush();
  ofs.close();
  return true;
}

std::optional<FlatMillingSolver> FlatMillingSolver::solve(Scene& scene, HeightMap& height_map,
                                                          const WorkpieceDesc& desc, float diameter) {
  // Find starting point
  static constexpr auto kMaxInd = std::numeric_limits<size_t>::max();
  struct PairHash {
    std::size_t operator()(const std::pair<size_t, size_t>& p) const noexcept {
      return std::hash<size_t>()(p.first) ^ (std::hash<size_t>()(p.second) << 1);
    }
  };

  const auto safe_depth = desc.max_depth + 2.F * diameter;

  // == Create border texture ==========================================================================================
  auto is_border     = std::vector<bool>();
  auto border_normal = std::unordered_map<std::pair<size_t, size_t>, math::Vec3f, PairHash>();
  is_border.resize(height_map.height * height_map.width, false);

  auto is_zero = [&height_map](size_t i, size_t j) { return height_map.height_map[height_map.width * i + j] < 1e-6F; };

  size_t last_found_i = kMaxInd;
  size_t last_found_j = kMaxInd;
  size_t border_count = 0;
  for (size_t i = 0; i < height_map.height; ++i) {
    for (size_t j = 0; j < height_map.width; ++j) {
      if (is_zero(i, j)) {
        continue;
      }

      if (i + 1 < height_map.height && is_zero(i + 1, j)) {
        is_border[height_map.width * (i + 1) + j] = true;
        last_found_i                              = i + 1;
        last_found_j                              = j;
        ++border_count;
        border_normal.emplace(std::make_pair(i + 1, j), height_map.normal_map[height_map.width * i + j]);
      }
      if (i > 0 && is_zero(i - 1, j)) {
        is_border[height_map.width * (i - 1) + j] = true;
        last_found_i                              = i - 1;
        last_found_j                              = j;
        ++border_count;
        border_normal.emplace(std::make_pair(i - 1, j), height_map.normal_map[height_map.width * i + j]);
      }
      if (j + 1 < height_map.width && is_zero(i, j + 1)) {
        is_border[height_map.width * i + j + 1] = true;
        last_found_i                            = i;
        last_found_j                            = j + 1;
        ++border_count;
        border_normal.emplace(std::make_pair(i, j + 1), height_map.normal_map[height_map.width * i + j]);
      }
      if (j > 0 && is_zero(i, j - 1)) {
        is_border[height_map.width * i + j - 1] = true;
        last_found_i                            = i;
        last_found_j                            = j - 1;
        ++border_count;
        border_normal.emplace(std::make_pair(i, j - 1), height_map.normal_map[height_map.width * i + j]);
      }
    }
  }

  if (last_found_i == kMaxInd || last_found_j == kMaxInd) {
    return std::nullopt;
  }

  // == Generate points ================================================================================================
  auto points = std::vector<math::Vec3f>();
  points.reserve(border_count);
  auto visited = std::unordered_set<std::pair<size_t, size_t>, PairHash>();
  for (size_t i = last_found_i, j = last_found_j, count = 0;;) {
    auto x = (static_cast<float>(j) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
    auto y = height_map.height_map[height_map.width * i + j];
    auto z = (static_cast<float>(i) / static_cast<float>(height_map.height) - 0.5F) * desc.height;

    auto norm      = border_normal.at(std::make_pair(i, j));
    auto flat_norm = math::Vec3f{norm.x, 0.F, norm.z}.normalize();

    points.emplace_back(math::Vec3f{x, y, z} + flat_norm * diameter / 2.F);

    for (size_t ii = 0; ii < 3; ++ii) {
      for (size_t jj = 0; jj < 3; ++jj) {
        if (ii == 1 && ii == jj) {
          continue;
        }
        auto iii = ii + i - 1;
        auto jjj = jj + j - 1;
        if (iii >= height_map.height || jjj >= height_map.width) {
          continue;
        }

        if (is_border[height_map.width * iii + jjj] && !visited.contains(std::make_pair(iii, jjj))) {
          visited.emplace(iii, jjj);
          i = iii;
          j = jjj;

          goto break_loop;  // 😈
        }
      }
    }
  break_loop:

    if (count >= border_count) {
      break;
    }

    ++count;
  }

  // == Find outer point ===============================================================================================
  const auto diag  = math::Vec2f{0.F, desc.height};
  const auto p     = math::Vec2f{0.F, desc.height / 2.F};
  size_t outer_ind = kMaxInd;
  for (auto i = 0U; i < points.size() - 1; ++i) {
    auto p0 = math::Vec2f{points[i].x, points[i].z};
    auto p1 = math::Vec2f{points[i + 1].x, points[i + 1].z};
    if (math::cross(p0 - p, diag) * math::cross(p1 - p, diag) > 0.F) {
      continue;
    }
    if (outer_ind == kMaxInd || points[outer_ind].z < points[i].z) {
      outer_ind = i;
    }
  }

  // == Remove self intersections ======================================================================================
  auto ind         = outer_ind;
  auto path_points = std::vector<math::Vec3f>();
  path_points.reserve(border_count);
  path_points.emplace_back(0.F, safe_depth, 0.F);
  path_points.emplace_back(0.F, safe_depth, desc.height / 2.F + diameter * 2.F);
  path_points.emplace_back(0.F, 0.F, desc.height / 2.F + diameter * 2.F);
  path_points.push_back(points[ind]);
  for (auto steps = 0U; steps < points.size(); ++steps) {
    auto next_ind = (ind + 1) % points.size();

    auto p0   = math::Vec2f{points[ind].x, points[ind].z};
    auto p1   = math::Vec2f{points[next_ind].x, points[next_ind].z};
    auto dirp = p1 - p0;

    auto old_ind = ind;
    ind          = next_ind;
    for (auto j = next_ind; j != old_ind; j = (j + 1) % points.size()) {
      auto next_j = (j + 1) % points.size();

      auto q0   = math::Vec2f{points[j].x, points[j].z};
      auto q1   = math::Vec2f{points[next_j].x, points[next_j].z};
      auto dirq = q1 - q0;

      auto c1 = math::cross(q0 - p1, dirp);
      auto c2 = math::cross(q1 - p1, dirp);
      auto c3 = math::cross(p0 - q1, dirq);
      auto c4 = math::cross(p1 - q1, dirq);

      if (c1 * c2 < 0.F && c3 * c4 < 0.F) {
        if (c1 > c2) {
          ind = j;
        } else {
          ind = j + 1;
        }

        break;
      }
    }

    path_points.push_back(points[ind]);
    if (ind == outer_ind) {
      break;
    }
  }
  path_points.emplace_back(points.back().x, safe_depth, points.back().z);
  path_points.emplace_back(0.F, safe_depth, 0.F);

  // == Upload border texture ==========================================================================================
  auto texture_temp = std::vector<eray::res::ColorU32>();
  texture_temp.resize(height_map.height * height_map.width);
  for (size_t i = 0; i < height_map.width * height_map.height; ++i) {
    texture_temp[i] = is_border[i] ? 0xFFFFFFFF : 0xFF000000;
  }

  auto handle = scene.renderer().upload_texture(texture_temp, height_map.height, height_map.width);
  return FlatMillingSolver{
      .points        = std::move(path_points),
      .border_handle = handle,
  };
}

}  // namespace mini
