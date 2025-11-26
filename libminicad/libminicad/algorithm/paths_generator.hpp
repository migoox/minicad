#pragma once

#include <filesystem>
#include <functional>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/patch_surface.hpp>
#include <vector>

#include "libminicad/scene/handles.hpp"

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
  struct Settings {
    bool dist_point_reduction = false;
    float dist_err            = 0.01F;
    bool param_space_offset   = false;
    bool dir                  = true;
    bool rdp_point_reduction  = true;
    int path_count            = 100;
  };

  /**
   * @brief Uses flat milling tool.
   *
   * @param height_map
   * @param desc
   * @param radius in centimeters
   */
  static std::optional<DetailedMillingSolver> solve(const HeightMap& height_map, Scene& scene,
                                                    const PatchSurfaceHandle& patch_handle, Settings settings,
                                                    const WorkpieceDesc& desc = WorkpieceDesc{}, float diameter = 0.8F);
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

struct IMillingPathsCombinerEntry {
  virtual ~IMillingPathsCombinerEntry() = default;

  virtual eray::math::Vec3f front()                                              = 0;
  virtual eray::math::Vec3f back()                                               = 0;
  virtual void append_to(std::vector<eray::math::Vec3f>& dest_vec, bool reverse) = 0;
  virtual size_t size()                                                          = 0;
};

struct PointObjectMillingPathCombinerEntry : public IMillingPathsCombinerEntry {
  PointObjectHandle point_handle;
  Scene* scene;  // scene lifetime is broader or the same as the combiner entry

  PointObjectMillingPathCombinerEntry() = delete;
  PointObjectMillingPathCombinerEntry(PointObjectHandle&& point_handle, Scene& scene)
      : point_handle(std::move(point_handle)), scene(&scene) {}

  static PointObjectMillingPathCombinerEntry create(PointObjectHandle handle, Scene& scene);

  eray::math::Vec3f front() override;
  eray::math::Vec3f back() override;
  void append_to(std::vector<eray::math::Vec3f>& dest_vec, bool reverse) override;
  size_t size() override;
};

struct PointArrayMillingPathCombinerEntry : public IMillingPathsCombinerEntry {
  std::vector<eray::math::Vec3f> points;

  PointArrayMillingPathCombinerEntry() = delete;
  explicit PointArrayMillingPathCombinerEntry(std::vector<eray::math::Vec3f>&& vec) : points(std::move(vec)) {}

  static PointArrayMillingPathCombinerEntry create(const std::vector<eray::math::Vec3f>& points);
  static PointArrayMillingPathCombinerEntry optimize_and_create(const std::vector<eray::math::Vec3f>& points,
                                                                float eps = 0.001F);
  static PointArrayMillingPathCombinerEntry from_approx_curve(const ApproxCurveHandle& handle, Scene& scene,
                                                              float eps = 0.001F);

  eray::math::Vec3f front() override;
  eray::math::Vec3f back() override;
  void append_to(std::vector<eray::math::Vec3f>& dest_vec, bool reverse) override;
  size_t size() override;
};

struct MillingPathsCombinerEntryInfo {
  std::string name;
  std::unique_ptr<IMillingPathsCombinerEntry> data;
  bool safe    = true;
  bool reverse = false;

  static MillingPathsCombinerEntryInfo from_entry(std::string&& name,
                                                  std::unique_ptr<IMillingPathsCombinerEntry>&& entry);
};

struct MillingPathsCombiner {
  std::vector<MillingPathsCombinerEntryInfo> paths;

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
  bool load_path_as_points(Scene& scene, const std::filesystem::path& filepath);
  void emplace_entry(MillingPathsCombinerEntryInfo&& entry) { paths.emplace_back(std::move(entry)); }

  std::vector<eray::math::Vec3f> combine(Scene& scene,
                                         std::optional<std::reference_wrapper<HeightMap>> height_map = std::nullopt,
                                         const WorkpieceDesc& desc                                   = WorkpieceDesc{});
};

struct GCodeSerializer {
  static bool write_to_file(Scene& scene, ApproxCurveHandle handle, const std::filesystem::path& filename,
                            float diameter = 0.8F, const WorkpieceDesc& desc = WorkpieceDesc{});

  static bool write_to_file(const std::vector<eray::math::Vec3f>& points, const std::filesystem::path& filename,
                            const WorkpieceDesc& desc = WorkpieceDesc{});
};

}  // namespace mini
