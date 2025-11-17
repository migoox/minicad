#pragma once

#include <filesystem>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <vector>

namespace mini {

namespace algo {
enum class Plane : uint8_t {
  XY,
  XZ,
  YZ,
  None,
};

/**
 * @brief RDP algorithm for points reduction.
 *
 * @param points
 * @param epsilon
 */
std::vector<eray::math::Vec3f> rdp(const std::vector<eray::math::Vec3f>& points, float epsilon,
                                   Plane plane = Plane::None);
void rdp_recursive(const std::vector<eray::math::Vec3f>& points, size_t start, size_t end, float epsilon, Plane plane,
                   std::vector<eray::math::Vec3f>& out);

struct ParamSurface {
  std::function<eray::math::Vec3f(float, float)> eval;
  std::function<std::pair<eray::math::Vec3f, eray::math::Vec3f>(float, float)> evald;
};

std::optional<eray::math::Vec3f> y_up_ray_vs_param_surface(Scene& scene, eray::math::Vec3f p0,
                                                           const ParamSurface& param_surface);
std::optional<eray::math::Vec3f> y_up_ray_vs_param_surfaces(eray::math::Vec3f p0,
                                                            const std::vector<ParamSurface>& param_surface);

}  // namespace algo

struct WorkpieceDesc {
  float width      = 15.F;
  float height     = 15.F;
  float depth      = 5.F;
  float max_depth  = 3.4F;
  float zero_level = 0.F;
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
                                                 float diameter = 1.6F, uint32_t layers = 2);
};

struct FlatMillingSolver {
  std::vector<eray::math::Vec3f> points;
  std::vector<std::vector<eray::math::Vec3f>> point_lists;
  TextureHandle border_handle;

  /**
   * @brief Uses flat milling tool.
   *
   * @param height_map
   * @param desc
   * @param radius in centimeters
   */
  static std::optional<FlatMillingSolver> solve(Scene& scene, HeightMap& height_map,
                                                const WorkpieceDesc& desc = WorkpieceDesc{}, float diameter = 1.0F,
                                                float contour_epsilon = 0.0095F);
};

struct DetailedMillingSolver {
  std::vector<std::vector<eray::math::Vec3f>> point_lists;
  TextureHandle trimming_texture;

  /**
   * @brief Uses flat milling tool.
   *
   * @param height_map
   * @param desc
   * @param radius in centimeters
   */
  static std::optional<DetailedMillingSolver> solve(const HeightMap& height_map, Scene& scene,
                                                    const PatchSurfaceHandle& patch_handle, bool dir = true,
                                                    size_t paths = 100, const WorkpieceDesc& desc = WorkpieceDesc{},
                                                    float diameter = 0.8F);
};

enum class PathType : std::uint8_t { Sphere, Flat };

struct MillingPath {
  std::optional<std::filesystem::path> filepath;
  std::vector<eray::math::Vec3f> points;  // [cm]
  PathType type;
  int diameter;  // [cm]
};

struct GCodeParser {
  static std::optional<MillingPath> parse(const std::filesystem::path& path,
                                          const WorkpieceDesc& desc = WorkpieceDesc{});
};

struct MillingPathsCombinerEntry {
  std::string name;
  std::variant<std::vector<eray::math::Vec3f>, eray::math::Vec3f> data;
  bool safe    = true;
  bool reverse = false;

  auto front() const {
    return std::visit(eray::util::match{[](const eray::math::Vec3f& p) { return p; },
                                        [](const std::vector<eray::math::Vec3f>& points) {
                                          if (points.empty()) {
                                            return eray::math::Vec3f::filled(0.F);
                                          }
                                          return points.front();
                                        }},
                      data);
  }

  auto back() const {
    return std::visit(eray::util::match{[](const eray::math::Vec3f& p) { return p; },
                                        [](const std::vector<eray::math::Vec3f>& points) {
                                          if (points.empty()) {
                                            return eray::math::Vec3f::filled(0.F);
                                          }
                                          return points.back();
                                        }},
                      data);
  }

  auto size() const {
    return std::visit(eray::util::match{[](const eray::math::Vec3f&) -> size_t { return 1U; },
                                        [](const std::vector<eray::math::Vec3f>& points) { return points.size(); }},
                      data);
  }

  auto append_to(std::vector<eray::math::Vec3f>& dest, bool rev = false) {
    std::visit(eray::util::match{[&dest](const eray::math::Vec3f& p) { dest.push_back(p); },
                                 [&dest, rev](const std::vector<eray::math::Vec3f>& points) {
                                   if (rev) {
                                     dest.append_range(points | std::views::reverse);
                                   } else {
                                     dest.append_range(points);
                                   }
                                 }},
               data);
  }
};

struct MillingPathsCombiner {
  std::vector<MillingPathsCombinerEntry> paths;

  float diameter          = 0.8F;
  PathType type           = PathType::Sphere;
  int unnamed_point_count = 0;
  int unnamed_path_count  = 0;

  /**
   * @brief
   *
   * @param filepath
   * @return true if successfully loaded
   * @return false
   */
  bool load_path(const std::filesystem::path& filepath);
  void emplace_point(eray::math::Vec3f&& point);
  void emplace_path(const MillingPath& path);

  std::vector<eray::math::Vec3f> combine(Scene& scene, const WorkpieceDesc& desc = WorkpieceDesc{});
};

struct GCodeSerializer {
  static bool write_to_file(const std::vector<eray::math::Vec3f>& points, const std::filesystem::path& filename,
                            const WorkpieceDesc& desc = WorkpieceDesc{});
};

}  // namespace mini
