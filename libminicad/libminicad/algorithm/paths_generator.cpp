#include <KHR/khrplatform.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <liberay/math/vec.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/logger.hpp>
#include <libminicad/algorithm/paths_generator.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene.hpp>
#include <limits>
#include <list>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "liberay/math/mat_fwd.hpp"
#include "liberay/math/vec_fwd.hpp"
#include "libminicad/scene/handles.hpp"
#include "libminicad/scene/param_primitive.hpp"
#include "libminicad/scene/trimming.hpp"

namespace mini {

namespace math = eray::math;

void algo::rdp_recursive(const std::vector<eray::math::Vec3f>& points, size_t start, size_t end, float epsilon,
                         Plane plane, std::vector<eray::math::Vec3f>& out) {
  if (end <= start + 1) {
    return;
  }

  auto project_onto_plane = [](math::Vec3f p, Plane plane) {
    switch (plane) {
      case Plane::XY:
        return math::Vec3f{p.x, p.y, 0.F};
      case Plane::XZ:
        return math::Vec3f{p.x, 0.F, p.z};
      case Plane::YZ:
        return math::Vec3f{0.F, p.y, p.z};
      case Plane::None:
        return p;
    }
  };

  float max_dist_sq = 0.0F;
  size_t pivot      = start;

  const auto dir      = project_onto_plane(points[end], plane) - project_onto_plane(points[start], plane);
  const auto dir_norm = dir.normalize();

  for (size_t i = start + 1; i < end; ++i) {
    const auto curr_dir = project_onto_plane(points[i], plane) - project_onto_plane(points[start], plane);
    auto dist_sq        = 0.F;
    if (std::abs(dir.x) < 1e-8F && std::abs(dir.y) < 1e-8F) {
      dist_sq = math::dot(curr_dir, curr_dir);
    } else {
      const auto h_dir = curr_dir - math::dot(curr_dir, dir_norm) * dir_norm;
      dist_sq          = math::dot(h_dir, h_dir);
    }

    if (dist_sq > max_dist_sq) {
      pivot       = i;
      max_dist_sq = dist_sq;
    }
  }

  if (max_dist_sq > epsilon * epsilon) {
    rdp_recursive(points, start, pivot, epsilon, plane, out);
    out.push_back(points[pivot]);
    rdp_recursive(points, pivot, end, epsilon, plane, out);
  }
}

std::vector<eray::math::Vec3f> algo::rdp(const std::vector<eray::math::Vec3f>& points, float epsilon, Plane plane) {
  if (points.size() < 2) {
    return points;
  }

  std::vector<math::Vec3f> result;
  result.reserve(points.size());
  result.push_back(points.front());
  rdp_recursive(points, 0, points.size() - 1, epsilon, plane, result);
  result.push_back(points.back());

  return result;
}

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
                                                            const float diameter, uint32_t layers) {
  const auto radius        = diameter / 2.F;
  const auto half_height   = desc.height / 2.F;
  const auto safety_offset = 1.2F * radius;

  const float right_x = desc.width / 2.F + safety_offset;
  const float left_x  = -desc.width / 2.F - safety_offset;

  auto fix_intersection =
      +[](const HeightMap& height_map, const WorkpieceDesc& desc, float radius, math::Vec3f& tool_tip) -> bool {
    const float x_right  = ((tool_tip.x + radius) / desc.width + 0.5F) * static_cast<float>(height_map.width);
    const float x_left   = ((tool_tip.x - radius) / desc.width + 0.5F) * static_cast<float>(height_map.width);
    const float z_top    = ((tool_tip.z + radius) / desc.height + 0.5F) * static_cast<float>(height_map.height);
    const float z_bottom = ((tool_tip.z - radius) / desc.height + 0.5F) * static_cast<float>(height_map.height);

    const auto map_right =
        static_cast<size_t>(std::clamp(std::lround(x_right), 0L, static_cast<long>(height_map.width)));
    const auto map_left = static_cast<size_t>(std::clamp(std::lround(x_left), 0L, static_cast<long>(height_map.width)));
    const auto map_top  = static_cast<size_t>(std::clamp(std::lround(z_top), 0L, static_cast<long>(height_map.height)));
    const auto map_bottom =
        static_cast<size_t>(std::clamp(std::lround(z_bottom), 0L, static_cast<long>(height_map.height)));

    auto max_map_y    = tool_tip.y;
    bool intersection = false;
    for (auto i = map_bottom; i < map_top; ++i) {
      for (auto j = map_left; j < map_right; ++j) {
        auto map_y = height_map.height_map[i * height_map.width + j];
        if (max_map_y < map_y) {
          intersection = true;
          max_map_y    = map_y;
        }
      }
    }
    tool_tip.y = max_map_y;
    return intersection;
  };

  auto points = std::vector<math::Vec3f>();
  points.emplace_back(0.F, safety_offset + desc.max_depth, 0.F);
  auto segment = std::vector<math::Vec3f>();

  float last_z = desc.height / 2.F;
  bool forward = true;
  points.emplace_back(right_x, safety_offset + desc.max_depth, last_z);
  for (auto i = 0U; i < layers; ++i) {
    const float layer_y = desc.max_depth - static_cast<float>(i + 1) * diameter;
    if (forward) {
      points.emplace_back(right_x, layer_y, last_z);
    }

    for (auto j = 0U;; ++j) {
      const float curr_z =
          i % 2 == 0 ? last_z - radius * static_cast<float>(j) : last_z + radius * static_cast<float>(j);

      if (forward) {
        points.emplace_back(right_x, layer_y, curr_z);
      } else {
        points.emplace_back(left_x, layer_y, curr_z);
      }

      auto map_i = static_cast<size_t>(static_cast<float>(height_map.height) * (curr_z / desc.height + 0.5F));
      if (map_i < height_map.height) {
        const int start = forward ? static_cast<int>(height_map.width) - 1 : 0;
        const int end   = forward ? -1 : static_cast<int>(height_map.width);
        const int step  = forward ? -1 : 1;

        segment.clear();
        auto last_not_intersecting_tool_tip = points.back();
        bool intersected                    = false;
        for (int map_j = start; map_j != end; map_j += step) {
          const float map_x = (static_cast<float>(map_j) / static_cast<float>(height_map.width) - 0.5F) * desc.width;
          auto tool_tip     = math::Vec3f{map_x, layer_y, curr_z};

          if (fix_intersection(height_map, desc, radius, tool_tip)) {
            if (!intersected) {
              segment.emplace_back(last_not_intersecting_tool_tip);
            }
            segment.emplace_back(tool_tip);
            intersected = true;
          } else {
            last_not_intersecting_tool_tip = tool_tip;
            intersected                    = false;
          }
        }
      }

      if (!segment.empty()) {
        auto reduced = algo::rdp(segment, 0.001F, algo::Plane::XY);
        points.append_range(reduced);
      }

      if (forward) {
        points.emplace_back(left_x, layer_y, curr_z);
      } else {
        points.emplace_back(right_x, layer_y, curr_z);
      }
      forward = !forward;

      if (i % 2 == 0) {
        if (curr_z <= -half_height) {
          last_z = curr_z;
          break;
        }
      } else {
        if (curr_z >= half_height) {
          last_z = curr_z;
          break;
        }
      }
    }
  }

  if (forward) {
    points.emplace_back(right_x, safety_offset + desc.max_depth, last_z);
  } else {
    points.emplace_back(left_x, safety_offset + desc.max_depth, last_z);
  }

  points.emplace_back(0.F, safety_offset + desc.max_depth, 0.F);
  return RoughMillingSolver{.points = std::move(points)};
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
                                                          const WorkpieceDesc& desc, float diameter,
                                                          float contour_epsilon) {
  // Find starting point
  static constexpr auto kMaxInd = std::numeric_limits<size_t>::max();
  struct PairHash {
    std::size_t operator()(const std::pair<size_t, size_t>& p) const noexcept {
      return std::hash<size_t>()(p.first) ^ (std::hash<size_t>()(p.second) << 1);
    }
  };

  // Describes the squared distance from line tolerance
  static constexpr auto kLineVsContourIntersectionTolerance = 0.0001F;

  const auto safe_depth = desc.max_depth + 2.F * diameter;

  // == Create border texture
  // ==========================================================================================
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

  // == Generate points
  // ================================================================================================
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
  };

  // == Find outer point
  // ===============================================================================================
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

  // == Remove self intersections
  // ======================================================================================
  auto ind            = outer_ind;
  auto contour_points = std::vector<math::Vec3f>();
  contour_points.reserve(border_count);
  contour_points.push_back(points[ind]);
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

    contour_points.push_back(points[ind]);
    if (ind == outer_ind) {
      break;
    }
  }
  contour_points = algo::rdp(contour_points, contour_epsilon, algo::Plane::XZ);

  // == Generate zig-zag segments ======================================================================================
  const auto intersection_find = +[](const math::Vec2f p0, const math::Vec2f p1, const math::Vec2f q0,
                                     const math::Vec2f q1) -> std::optional<math::Vec2f> {
    const auto p01 = p1 - p0;
    const auto q01 = q1 - q0;
    const auto c0  = math::cross(q0 - p1, p01);
    const auto c1  = math::cross(q1 - p1, p01);
    const auto c2  = math::cross(p0 - q1, q01);
    const auto c3  = math::cross(p1 - q1, q01);
    if (c0 * c1 < 0.F && c2 * c3 < 0.F) {
      const auto w   = math::cross(p01, -q01);
      const auto w_u = math::cross(q0 - p0, -q01);
      return p0 + w_u / w * p01;
    }
    return std::nullopt;
  };

  struct ZigZagSegment {
    size_t i = 0;  // line index
    size_t j = 0;  // subdivision index, 0 if no subdivision
    math::Vec2f start;
    math::Vec2f end;
    size_t contour_index_start = kMaxInd;
    size_t contour_index_end   = kMaxInd;
  };
  struct IntersectionPoint {
    size_t contour_index{};
    math::Vec2f pos;
  };

  const auto diameter_eps = diameter - 0.08F;
  const float right_x     = desc.width / 2.F + diameter * 2.F;
  const float left_x      = -desc.width / 2.F - diameter * 2.F;
  const float start_z     = desc.height / 2.F;

  auto segments            = std::vector<std::list<ZigZagSegment>>();
  auto intersection_points = std::vector<IntersectionPoint>();
  for (auto i = 0U;; ++i) {
    float curr_z = start_z - diameter_eps * static_cast<float>(i);

    auto p0 = math::Vec2f{right_x, curr_z};
    auto p1 = math::Vec2f{left_x, curr_z};

    intersection_points.clear();
    for (auto j = 0U; j < contour_points.size() - 1; ++j) {
      auto q0 = math::Vec2f{contour_points[j].x, contour_points[j].z};
      auto q1 = math::Vec2f{contour_points[j + 1].x, contour_points[j + 1].z};

      if (auto result = intersection_find(p0, p1, q0, q1)) {
        intersection_points.emplace_back(j, *result);
      }
    }
    std::ranges::sort(intersection_points, [](const auto& a, const auto& b) { return a.pos.x > b.pos.x; });

    segments.emplace_back();

    auto prev             = p0;
    auto prev_contour_ind = kMaxInd;
    auto j                = 0U;
    for (; j < intersection_points.size(); j += 2) {
      segments[i].emplace_front(ZigZagSegment{
          .i                   = i,
          .j                   = j,
          .start               = prev,
          .end                 = intersection_points[j].pos,
          .contour_index_start = prev_contour_ind,
          .contour_index_end   = intersection_points[j].contour_index,
      });
      if (segments[i].back().start.x < segments[i].back().end.x) {
        std::swap(segments[i].back().start, segments[i].back().end);
        std::swap(segments[i].back().contour_index_start, segments[i].back().contour_index_end);
      }
      prev             = intersection_points[j + 1].pos;
      prev_contour_ind = intersection_points[j + 1].contour_index;
    }
    segments[i].emplace_front(ZigZagSegment{
        .i                   = i,
        .j                   = j,
        .start               = prev,
        .end                 = p1,
        .contour_index_start = prev_contour_ind,
        .contour_index_end   = kMaxInd,
    });
    if (segments[i].back().start.x < segments[i].back().end.x) {
      std::swap(segments[i].back().start, segments[i].back().end);
      std::swap(segments[i].back().contour_index_start, segments[i].back().contour_index_end);
    }
    if (curr_z < -desc.height / 2.F) {
      break;
    }
  }

  // == Generate paths =================================================================================================
  auto path_points = std::vector<math::Vec3f>();
  path_points.emplace_back(0.F, safe_depth, 0.F);
  path_points.emplace_back(0.F, safe_depth, desc.height / 2.F + diameter * 2.F);
  path_points.emplace_back(0.F, 0.F, desc.height / 2.F + diameter * 2.F);
  path_points.reserve(path_points.size() + contour_points.size());
  for (const auto& cp : contour_points) {
    if (std::isfinite(cp.x) && std::isfinite(cp.y) && std::isfinite(cp.z)) {
      path_points.push_back(cp);
    }
  }
  path_points.emplace_back(contour_points.back().x, safe_depth, contour_points.back().z);
  for (;;) {
    auto first_line_ind = kMaxInd;
    for (auto i = 0U; i < segments.size() - 1; ++i) {
      if (!segments[i].empty()) {
        first_line_ind = i;
        break;
      }
    }

    if (first_line_ind == kMaxInd) {
      break;
    }

    auto previous_segment = segments[first_line_ind].begin();
    while (std::next(previous_segment) != segments[first_line_ind].end()) {
      ++previous_segment;
    }

    auto rest = first_line_ind % 2;
    path_points.emplace_back(previous_segment->start.x, safe_depth, previous_segment->start.y);
    path_points.emplace_back(previous_segment->start.x, 0.F, previous_segment->start.y);
    path_points.emplace_back(previous_segment->end.x, 0.F, previous_segment->end.y);

    auto i = first_line_ind;
    for (;;) {
      if (i + 1 >= segments.size()) {
        break;
      }
      if (segments[i + 1].empty()) {
        break;
      }

      ++i;

      const bool forward = i % 2 == rest;
      auto curr_segment  = segments[i].end();
      auto min_val       = std::numeric_limits<float>::max();
      for (auto segment_it = segments[i].begin(); segment_it != segments[i].end(); ++segment_it) {
        // TODO(migoox): Note that the code below is an approximation, to have accurate results, the contour length
        // should be used.
        auto val =
            forward
                ? math::dot(segment_it->start - previous_segment->start, segment_it->start - previous_segment->start)
                : math::dot(segment_it->end - previous_segment->end, segment_it->end - previous_segment->end);
        if (val < min_val) {
          min_val      = val;
          curr_segment = segment_it;
        }
      }

      // Fix line intersection with the contour
      size_t start_ind = kMaxInd;
      size_t end_ind   = kMaxInd;
      auto start       = math::Vec2f::filled(0.F);
      auto end         = math::Vec2f::filled(0.F);
      if (forward) {
        if (previous_segment->contour_index_start != kMaxInd && curr_segment->contour_index_start != kMaxInd) {
          start_ind = previous_segment->contour_index_start;
          end_ind   = curr_segment->contour_index_start;
          start     = previous_segment->start;
          end       = curr_segment->start;
        }
      } else {
        if (previous_segment->contour_index_end != kMaxInd && curr_segment->contour_index_end != kMaxInd) {
          start_ind = previous_segment->contour_index_end;
          end_ind   = curr_segment->contour_index_end;
          start     = previous_segment->end;
          end       = curr_segment->end;
        }
      }

      if (start_ind != kMaxInd && end_ind != kMaxInd) {
        const auto n = contour_points.size();

        auto forward_length_sq = 0.F;
        for (size_t l = 0U, forward_ind = start_ind, temp_forward_ind = forward_ind; l < n; ++l) {
          if (forward_ind == end_ind) {
            break;
          }

          temp_forward_ind = forward_ind;
          forward_ind      = (forward_ind + 1) % n;
          forward_length_sq += math::dot(contour_points[forward_ind] - contour_points[temp_forward_ind],
                                         contour_points[forward_ind] - contour_points[temp_forward_ind]);
        }

        auto backward_length_sq = 0.F;
        for (size_t l = 0U, backward_ind = start_ind, temp_backward_ind = backward_ind; l < n; ++l) {
          if (backward_ind == end_ind) {
            break;
          }

          temp_backward_ind = backward_ind;
          backward_ind      = (n + backward_ind - 1) % n;  // avoid negative number
          backward_length_sq += math::dot(contour_points[backward_ind] - contour_points[temp_backward_ind],
                                          contour_points[backward_ind] - contour_points[temp_backward_ind]);
        }

        const bool swapped = backward_length_sq < forward_length_sq;
        if (swapped) {
          std::swap(start_ind, end_ind);
          std::swap(start, end);
        }

        const auto dir      = end - start;
        const auto norm_dir = dir.normalize();

        if (swapped) {
          for (size_t l = 0U, backward_ind = (n + end_ind - 1) % n; l < n; ++l) {
            if (backward_ind == start_ind) {
              break;
            }
            const auto curr_dir = math::Vec2f{contour_points[backward_ind].x, contour_points[backward_ind].z} - start;
            const auto c        = math::cross(dir, curr_dir);
            if (c < 0.F) {
              auto h_dir = curr_dir - norm_dir * math::dot(curr_dir, norm_dir);
              if (math::dot(h_dir, h_dir) > kLineVsContourIntersectionTolerance) {
                path_points.emplace_back(contour_points[backward_ind]);  // fix intersection
              }
            }
            backward_ind = (n + backward_ind - 1) % n;
          }
        } else {
          for (size_t l = 0U, forward_ind = (start_ind + 1) % n; l < n; ++l) {
            if (forward_ind == end_ind) {
              break;
            }
            const auto curr_dir = math::Vec2f{contour_points[forward_ind].x, contour_points[forward_ind].z} - start;
            const auto c        = math::cross(dir, curr_dir);
            if (c < 0.F) {
              auto h_dir = curr_dir - norm_dir * math::dot(curr_dir, norm_dir);
              if (math::dot(h_dir, h_dir) > kLineVsContourIntersectionTolerance) {
                path_points.emplace_back(contour_points[forward_ind]);  // fix intersection
              }
            }
            forward_ind = (forward_ind + 1) % n;
          }
        }
      }

      if (forward) {
        path_points.emplace_back(curr_segment->start.x, 0.F, curr_segment->start.y);
        path_points.emplace_back(curr_segment->end.x, 0.F, curr_segment->end.y);
      } else {
        path_points.emplace_back(curr_segment->end.x, 0.F, curr_segment->end.y);
        path_points.emplace_back(curr_segment->start.x, 0.F, curr_segment->start.y);
      }

      segments[i - 1].erase(previous_segment);
      previous_segment = curr_segment;
    }
    if (i % 2 == rest) {
      path_points.emplace_back(previous_segment->end.x, safe_depth, previous_segment->end.y);
    } else {
      path_points.emplace_back(previous_segment->start.x, safe_depth, previous_segment->start.y);
    }
    segments[i].erase(previous_segment);
  }

  //   for (auto i = 0U; i < contour_points.size() - 1; ++i) {
  //     scene.renderer().debug_line(contour_points[i], contour_points[i + 1]);
  //   }
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

namespace {

constexpr std::optional<eray::math::Mat3f> inverse(const eray::math::Mat3f& m) {
  const float coef00 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
  const float coef01 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
  const float coef02 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

  const float det = m[0][0] * coef00 - m[0][1] * coef01 + m[0][2] * coef02;

  if (std::abs(det) < 0.000001F) {
    return std::nullopt;
  }

  eray::math::Mat3f inv;

  inv[0][0] = (m[1][1] * m[2][2] - m[2][1] * m[1][2]);
  inv[0][1] = -(m[0][1] * m[2][2] - m[2][1] * m[0][2]);
  inv[0][2] = (m[0][1] * m[1][2] - m[1][1] * m[0][2]);

  inv[1][0] = -(m[1][0] * m[2][2] - m[2][0] * m[1][2]);
  inv[1][1] = (m[0][0] * m[2][2] - m[2][0] * m[0][2]);
  inv[1][2] = -(m[0][0] * m[1][2] - m[1][0] * m[0][2]);

  inv[2][0] = (m[1][0] * m[2][1] - m[2][0] * m[1][1]);
  inv[2][1] = -(m[0][0] * m[2][1] - m[2][0] * m[0][1]);
  inv[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]);

  return inv * (1.F / det);
}

}  // namespace

namespace algo {

static constexpr float wrap_to_unit_interval(float x) noexcept {
  float int_part        = 0.F;
  const float frac_part = std::modf(x, &int_part);
  return frac_part < 0.0F ? frac_part + 1.0F : frac_part;
}

static std::optional<eray::math::Vec3f> y_up_ray_vs_param_surface(Scene& scene, eray::math::Vec3f p0,
                                                                  ParamSurface& param_surface) {
  auto u = 0.5F;
  auto v = 0.5F;
  auto t = 0.5F;

  for (auto i = 0U; i < 100; ++i) {
    auto diff = param_surface.eval(u, v) - p0;
    diff.y -= t;
    if (math::dot(diff, diff) < 0.01F * 0.01F) {
      break;
    }
  }

  //   constexpr auto kIters = 10U;

  //   auto u = 0.5F;
  //   auto v = 0.5F;
  //   auto t = 5.F;

  //   for (auto i = 0U; i < kIters; ++i) {
  //     auto val   = param_surface.eval(u, v);
  //     auto deriv = param_surface.evald(u, v);

  //     auto jacobian = math::Mat3f{deriv.first, deriv.second, math::Vec3f{0.F, -1.F, 0.F}};
  //     if (auto inv_jacobian = inverse(jacobian)) {
  //       auto func_val = val - p0;
  //       func_val.y -= t;
  //       auto new_params = math::Vec3f{u, v, t} - *inv_jacobian * func_val;
  //       u               = wrap_to_unit_interval(new_params.x);
  //       v               = wrap_to_unit_interval(new_params.y);
  //       t               = new_params.z;
  //     } else {
  //       // perturb
  //       u += 0.01F;
  //       v += 0.01F;
  //       t += 0.01F;
  //     }
  //   }
  //   scene.renderer().debug_point(param_surface.eval(u, v));

  return math::Vec3f{u, v, t};
}

std::optional<eray::math::Vec3f> y_up_ray_vs_param_surfaces(eray::math::Vec3f p0,
                                                            const std::vector<ParamSurface>& param_surface) {
  return std::nullopt;
}

}  // namespace algo

static void line(int x0, int y0, int x1, int y1, std::function<void(int x, int y)> on_pixel) {
  constexpr auto kTrimmingTextureSizeInt = static_cast<int>(ParamSpaceTrimmingData::kCPUTrimmingTxtSize);

  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);

  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;

  int err = dx - dy;

  while (true) {
    if (x0 >= 0 && x0 < kTrimmingTextureSizeInt && y0 >= 0 && y0 < kTrimmingTextureSizeInt) {
      on_pixel(x0, y0);
    }

    if (x0 == x1 && y0 == y1) {
      break;
    }

    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

std::optional<DetailedMillingSolver> DetailedMillingSolver::solve(Scene& scene, const PatchSurfaceHandle& patch_handle,
                                                                  bool dir, size_t paths, const WorkpieceDesc& desc,
                                                                  float diameter) {
  constexpr auto kTrimmingTextureSize    = ParamSpaceTrimmingData::kCPUTrimmingTxtSize;
  constexpr auto kTrimmingTextureSizeInt = static_cast<int>(ParamSpaceTrimmingData::kCPUTrimmingTxtSize);
  constexpr auto kTrimmingTextureSizeFlt = static_cast<float>(ParamSpaceTrimmingData::kCPUTrimmingTxtSize);

  const auto radius       = diameter / 2.F;
  const auto safety_depth = desc.depth + 1.2F * radius;

  if (!scene.arena<PatchSurface>().exists(patch_handle)) {
    eray::util::Logger::warn("Could not generate the detailed paths: no patch surfaces provided");
    return std::nullopt;
  }
  auto patch_surface = scene.arena<PatchSurface>().unsafe_get_obj(patch_handle);

  // == Get trimming intersecting surfaces info ========================================================================
  std::vector<ObserverPtr<PatchSurface>> intersecting_surfaces;
  for (auto handle : patch_surface->trimming_manager().final_intersecting_surfaces_handles()) {
    bool failure = false;
    std::visit(eray::util::match{
                   [&](const PatchSurfaceHandle& h) {
                     if (auto o = scene.arena<PatchSurface>().get_obj(h)) {
                       intersecting_surfaces.emplace_back(*o.value());
                     } else {
                       eray::util::Logger::err("One of the intersecting surfaces no longer exists");
                       failure = true;
                     }
                   },
                   [&](const auto&) {
                     eray::util::Logger::err("Only patch surfaces are supported");
                     failure = true;
                   },
               },
               handle);

    if (failure) {
      return std::nullopt;
    }
  }

  auto is_below_level = [&](size_t i, size_t j) {
    const auto u = static_cast<float>(j) / kTrimmingTextureSizeFlt;
    const auto v = static_cast<float>(i) / kTrimmingTextureSizeFlt;
    auto p       = patch_surface->evaluate(u, v);
    auto deriv   = patch_surface->evaluate_derivatives(u, v);
    p            = p + math::cross(deriv.first, deriv.second).normalize() * radius;
    return p.y < desc.zero_level + radius + 0.01F;
  };

  std::vector<algo::ParamSurface> intersecting_param_surfaces;
  for (auto i = 0U; i < intersecting_surfaces.size(); ++i) {  // NOLINT
    auto eval = [&intersecting_surfaces, i, radius](float u, float v) {
      auto deriv      = intersecting_surfaces[i]->evaluate_derivatives(u, v);
      auto offset_vec = eray::math::cross(deriv.first, deriv.second).normalize() * radius;
      return intersecting_surfaces[i]->evaluate(u, v) + offset_vec;
    };
    auto evald = [&intersecting_surfaces, i, radius](float u, float v) {
      auto deriv  = intersecting_surfaces[i]->evaluate_derivatives(u, v);
      auto deriv2 = intersecting_surfaces[i]->evaluate_second_derivatives(u, v);

      auto dn_du =
          (eray::math::cross(deriv2.dp_duu, deriv.second) + eray::math::cross(deriv.first, deriv2.dp_duv)).normalize();
      auto dn_dv =
          (eray::math::cross(deriv2.dp_duv, deriv.second) + eray::math::cross(deriv.first, deriv2.dp_dvv)).normalize();

      auto result = intersecting_surfaces[i]->evaluate_derivatives(u, v);
      result.first += radius * dn_du;
      result.second += radius * dn_dv;
      return result;
    };
    intersecting_param_surfaces.emplace_back(algo::ParamSurface{.eval = std::move(eval), .evald = std::move(evald)});
  }

  auto trimming_txt = patch_surface->trimming_manager().final_txt();
  for (size_t i = 0U; i < kTrimmingTextureSize; ++i) {
    for (size_t j = 0U; j < kTrimmingTextureSize; ++j) {
      if (is_below_level(i, j)) {
        trimming_txt[i * kTrimmingTextureSize + j] = 0xFF000000;
      }
    }
  }

  // ===================================================================================================================

  // j (column) -> u parameter
  // i (row) -> v

  // == Generate line segments =========================================================================================
  struct Coord {
    int i = 0;
    int j = 0;
  };
  std::vector<std::vector<Coord>> lines;
  {
    lines.reserve(paths);

    auto mark = [&](size_t i, size_t j) {
      const auto u = static_cast<float>(j) / kTrimmingTextureSizeFlt;
      const auto v = static_cast<float>(i) / kTrimmingTextureSizeFlt;
      auto p       = patch_surface->evaluate(u, v);
      auto deriv   = patch_surface->evaluate_derivatives(u, v);
      //   scene.renderer().debug_point(p + math::cross(deriv.first, deriv.second).normalize() * radius);
    };

    auto is_trimmed = [&](size_t i, size_t j) { return trimming_txt[i * kTrimmingTextureSize + j] == 0xFF000000; };

    const auto i_step = kTrimmingTextureSize / paths;
    for (size_t i_base = 0U; i_base < kTrimmingTextureSize; i_base += i_step) {
      std::vector<Coord> line;

      for (size_t j_base = 0U; j_base < kTrimmingTextureSize; ++j_base) {
        auto i  = i_base;
        auto j  = j_base;
        auto in = i_base;
        auto jn = static_cast<size_t>(j_base + 1U);
        if (dir) {
          std::swap(i, j);
          in = j_base + 1;
          jn = i_base;
        }

        if ((j_base == 0 || j_base == kTrimmingTextureSize - 1) && !is_trimmed(i, j)) {
          line.emplace_back(i, j);
          mark(i, j);
        }

        if (j_base < kTrimmingTextureSize - 1 && is_trimmed(i, j) != is_trimmed(in, jn)) {
          line.emplace_back(i, j);
          mark(i, j);
        }
      }
      lines.emplace_back(std::move(line));
    }
  }

  // Texture visualization
  for (auto& line : lines) {
    if (line.size() % 2 != 0) {
      eray::util::Logger::err("Error");
      return std::nullopt;
    }
    for (auto k = 0U; k < line.size(); k += 2U) {
      if (line[k].i == line[k + 1].i) {
        for (auto j = line[k].j; j <= line[k + 1].j; ++j) {
          trimming_txt[line[k].i * kTrimmingTextureSize + j] = 0xFF00FFFF;
        }
      } else {
        for (auto i = line[k].i; i <= line[k + 1].i; ++i) {
          trimming_txt[i * kTrimmingTextureSize + line[k].j] = 0xFF00FFFF;
        }
      }
    }
  }

  // == Generate paths =================================================================================================
  auto evaluate = [&](size_t i, size_t j) {
    const auto u = static_cast<float>(j) / kTrimmingTextureSizeFlt;
    const auto v = static_cast<float>(i) / kTrimmingTextureSizeFlt;
    auto p       = patch_surface->evaluate(u, v);
    auto deriv   = patch_surface->evaluate_derivatives(u, v);
    return p + math::cross(deriv.first, deriv.second).normalize() * radius;
  };

  std::vector<std::vector<eray::math::Vec3f>> point_lists;
  auto points                 = std::vector<math::Vec3f>();
  auto current_segment_points = std::vector<math::Vec3f>();

  auto draw_straight_line = [&](Coord start, Coord end) {
    current_segment_points.clear();

    if (start.i == end.i) {
      if (start.j < end.j) {
        for (auto j = start.j; j <= end.j; ++j) {
          current_segment_points.push_back(evaluate(start.i, j));
        }
      } else {
        for (auto j = start.j; j >= end.j; --j) {
          current_segment_points.push_back(evaluate(start.i, j));
        }
      }
    } else {
      if (start.i < end.i) {
        for (auto i = start.i; i <= end.i; ++i) {
          current_segment_points.push_back(evaluate(i, start.j));
        }
      } else {
        for (auto i = start.i; i >= end.i; --i) {
          current_segment_points.push_back(evaluate(i, start.j));
        }
      }
    }
    points.append_range(algo::rdp(current_segment_points, 0.001F));
  };

  auto debug_straight_line = [&](Coord start, Coord end) {
    if (start.i == end.i) {
      for (auto j = std::min(start.j, end.j); j < std::min(start.j, end.j); ++j) {
        scene.renderer().debug_line(evaluate(start.i, j), evaluate(start.i, j + 1));
      }
    } else {
      for (auto i = std::min(start.i, end.i); i < std::max(start.i, end.i); ++i) {
        scene.renderer().debug_line(evaluate(i, start.j), evaluate(i + 1, start.j));
      }
    }
  };

  auto on_draw = [&](int j, int i) {
    if (trimming_txt[i * kTrimmingTextureSize + j] == 0xFF000000) {
      // find closest non-trimmed pixel
      int radius = 1;
      bool found = false;
      while (!found) {
        bool hit_border = false;

        for (int dj = -radius; dj <= radius && !found; ++dj) {
          for (int di = -radius; di <= radius && !found; ++di) {
            int nj = j + dj;
            int ni = i + di;

            if (nj < 0 || nj >= kTrimmingTextureSize || ni < 0 || ni >= kTrimmingTextureSize) {
              hit_border = true;
              break;
            }

            if (trimming_txt[ni * kTrimmingTextureSize + nj] != 0xFF000000) {
              trimming_txt[ni * kTrimmingTextureSize + nj] = 0xFF00FFFF;
              current_segment_points.push_back(evaluate(ni, nj));
              found = true;
              break;
            }
          }
        }

        if (found || hit_border) {
          break;
        }
        radius++;
      }
    } else {
      trimming_txt[i * kTrimmingTextureSize + j] = 0xFF00FFFF;
      current_segment_points.push_back(evaluate(i, j));
    }
  };

  bool forward = true;
  for (;;) {
    points.clear();
    auto first_line_ind = lines.size();
    for (auto i = 0U; i < lines.size() - 1; ++i) {
      if (!lines[i].empty()) {
        first_line_ind = i;
        break;
      }
    }

    if (first_line_ind == lines.size()) {
      break;
    }

    auto prev_segment_start = lines[first_line_ind][lines[first_line_ind].size() - 2];
    auto prev_segment_end   = lines[first_line_ind][lines[first_line_ind].size() - 1];
    lines[first_line_ind].pop_back();
    lines[first_line_ind].pop_back();
    if (forward) {
      draw_straight_line(prev_segment_start, prev_segment_end);
    } else {
      draw_straight_line(prev_segment_end, prev_segment_start);
    }
    forward = !forward;

    for (auto line_ind = first_line_ind + 1; line_ind < lines.size(); ++line_ind) {
      if (lines[line_ind].empty()) {
        break;
      }

      // Find best next segment from current line
      auto min              = std::numeric_limits<float>::max();
      auto best_segment_ind = 0U;
      for (auto segment_ind = 0; segment_ind < lines[line_ind].size(); segment_ind += 2) {
        if (forward) {
          auto curr_segment_end = lines[line_ind][segment_ind + 1];
          auto prev             = evaluate(prev_segment_end.i, prev_segment_end.j);
          auto curr             = evaluate(curr_segment_end.i, curr_segment_end.j);

          auto curr_dist = dot(prev - curr, prev - curr);
          if (min < curr_dist) {
            min              = curr_dist;
            best_segment_ind = segment_ind;
          }
        } else {
          auto curr_segment_start = lines[line_ind][segment_ind];
          auto prev               = evaluate(prev_segment_start.i, prev_segment_start.j);
          auto curr               = evaluate(curr_segment_start.i, curr_segment_start.j);

          auto curr_dist = dot(prev - curr, prev - curr);
          if (min < curr_dist) {
            min              = curr_dist;
            best_segment_ind = segment_ind;
          }
        }
      }

      auto segment_start = lines[line_ind][best_segment_ind];
      auto segment_end   = lines[line_ind][best_segment_ind + 1];
      lines[line_ind].erase(lines[line_ind].begin() + best_segment_ind, lines[line_ind].begin() + best_segment_ind + 2);

      if (forward) {
        current_segment_points.clear();
        line(prev_segment_start.j, prev_segment_start.i, segment_start.j, segment_start.i, on_draw);
        points.append_range(algo::rdp(current_segment_points, 0.001F));
        draw_straight_line(segment_start, segment_end);
      } else {
        current_segment_points.clear();
        line(prev_segment_end.j, prev_segment_end.i, segment_end.j, segment_end.i, on_draw);
        points.append_range(algo::rdp(current_segment_points, 0.001F));
        draw_straight_line(segment_end, segment_start);
      }
      forward = !forward;

      prev_segment_start = segment_start;
      prev_segment_end   = segment_end;
    }

    if (!points.empty()) {
      point_lists.push_back(points);
    }
  }

  if (point_lists.empty()) {
    return std::nullopt;
  }

  // Transform tool center to tool tip
  //   for (auto& point : points) {
  //     point.y += radius;
  //   }

  //   const auto a = desc.zero_level + radius;
  //   if (p[0].y > a && p[1].y > a) {
  //     points.push_back(p[0]);
  //     points.push_back(p[1]);

  //   } else if (p[0].y > a) {
  //     const auto t         = (a - p[0].y) / (p[1].y - p[0].y);
  //     const auto projected = p[0] + t * (p[1] - p[0]);
  //     points.push_back(p[0]);
  //     points.push_back(projected);

  //   } else if (p[1].y > a) {
  //     const auto t         = (a - p[1].y) / (p[0].y - p[1].y);
  //     const auto projected = p[1] + t * (p[0] - p[1]);
  //     points.push_back(projected);
  //     points.push_back(p[1]);
  //   }

  auto handle = scene.renderer().upload_texture(trimming_txt, ParamSpaceTrimmingData::kCPUTrimmingTxtSize,
                                                ParamSpaceTrimmingData::kCPUTrimmingTxtSize);

  // == Center to tooltip transform ====================================================================================
  for (auto& point_list : point_lists) {
    for (auto& p : point_list) {
      p.y -= radius;
    }
  }

  return DetailedMillingSolver{
      .point_lists      = std::move(point_lists),
      .trimming_texture = handle,
  };
}

}  // namespace mini
