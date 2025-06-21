#pragma once
#include <liberay/math/vec.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/scene/point_list.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/types.hpp>

namespace mini {

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurfaceType --------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct PlanePatchSurfaceStarter {
  eray::math::Vec2f size;
};

struct CylinderPatchSurfaceStarter {
  float radius;
  float height;
  float phase;
};

using PatchSurfaceStarter = std::variant<PlanePatchSurfaceStarter, CylinderPatchSurfaceStarter>;

struct BezierPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Bezier Patches"; }
  static void set_control_points(PointList& points, const PatchSurfaceStarter& starter, eray::math::Vec2u dim);
  [[nodiscard]] static eray::math::Vec2u control_points_dim(eray::math::Vec2u patches_dim);
  [[nodiscard]] static eray::math::Vec2u patches_dim(eray::math::Vec2u points_dim);
  [[nodiscard]] static eray::math::Vec2u unique_control_points_dim(const PatchSurfaceStarter& starter,
                                                                   eray::math::Vec2u patches_dim);

  /**
   * @brief Every 17th point stores info about tesselation.
   *
   * @param bezier
   */
  void update_bezier3_points(PatchSurface& base);

  static size_t find_idx(size_t patch_x, size_t patch_y, size_t point_x, size_t point_y, size_t dim_x);
  static size_t find_patch_offset(size_t patch_x, size_t patch_y, size_t dim_x);
  static size_t find_size_x(size_t dim_x);
};

struct BPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "B-Patches"; }
  static void set_control_points(PointList& points, const PatchSurfaceStarter& starter, eray::math::Vec2u dim);
  [[nodiscard]] static eray::math::Vec2u control_points_dim(eray::math::Vec2u patches_dim);
  [[nodiscard]] static eray::math::Vec2u patches_dim(eray::math::Vec2u points_dim);
  [[nodiscard]] static eray::math::Vec2u unique_control_points_dim(const PatchSurfaceStarter& starter,
                                                                   eray::math::Vec2u patches_dim);

  /**
   * @brief Every 17th point stores info about tesselation.
   *
   * @param bezier
   */
  void update_bezier3_points(PatchSurface& base);

  static size_t find_idx(size_t patch_x, size_t patch_y, size_t point_x, size_t point_y, size_t dim_x);
  static size_t find_patch_offset(size_t patch_x, size_t patch_y, size_t dim_x);
  static size_t find_size_x(size_t dim_x);
};

template <typename T>
concept CPatchSurfaceType =
    requires(T t, PatchSurface& base, eray::math::Vec2u dim, const PatchSurfaceStarter& starter_ref,
             const PatchSurfaceStarter& starter, PointList& points, size_t idx) {
      { T::type_name() } -> std::same_as<zstring_view>;
      { T::set_control_points(points, starter, dim) } -> std::same_as<void>;
      { T::control_points_dim(dim) } -> std::same_as<eray::math::Vec2u>;
      { T::patches_dim(dim) } -> std::same_as<eray::math::Vec2u>;
      { T::unique_control_points_dim(starter_ref, dim) } -> std::same_as<eray::math::Vec2u>;
      { T::find_idx(idx, idx, idx, idx, idx) } -> std::same_as<size_t>;
      { T::find_patch_offset(idx, idx, idx) } -> std::same_as<size_t>;
      { T::find_size_x(idx) } -> std::same_as<size_t>;
      { t.update_bezier3_points(base) } -> std::same_as<void>;
    };

using PatchSurfaceVariant = std::variant<BezierPatches, BPatches>;
MINI_VALIDATE_VARIANT_TYPES(PatchSurfaceVariant, CPatchSurfaceType);

// ---------------------------------------------------------------------------------------------------------------------
// - PatchSurface ------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class PatchSurface : public ObjectBase<PatchSurface, PatchSurfaceVariant>, public PointListObjectBase<PatchSurface> {
 public:
  PatchSurface() = delete;
  PatchSurface(const PatchSurfaceHandle& handle, Scene& scene);

  ERAY_DEFAULT_MOVE(PatchSurface)
  ERAY_DELETE_COPY(PatchSurface)

  static constexpr int kPatchSize        = 4;
  static constexpr int kDefaultTessLevel = 4;

  enum class GetterError : uint8_t {
    OutOfBounds = 0,
  };

  [[nodiscard]] std::expected<std::array<std::array<SceneObjectHandle, kPatchSize>, kPatchSize>, GetterError>
  patch_control_point_handles(eray::math::Vec2u patch_coords) const;

  [[nodiscard]] std::array<std::array<SceneObjectHandle, kPatchSize>, kPatchSize> unsafe_patch_control_point_handles(
      eray::math::Vec2u patch_coords) const;

  auto patches_control_point_handles() const {
    return std::views::cartesian_product(std::views::iota(0U, dim_.y), std::views::iota(0U, dim_.x)) |
           std::views::transform([this](auto&& pair) {
             auto [y, x] = pair;
             auto coords = eray::math::Vec2u(x, y);
             return std::make_pair(unsafe_patch_control_point_handles(coords), coords);
           });
  }

  std::generator<eray::math::Vec3f> control_grid_points() const;
  size_t control_grid_points_count() const;

  const std::vector<eray::math::Vec3f>& bezier3_points();

  enum class InitError : uint8_t {
    PointsAndDimensionsMismatch = 0,
    SceneObjectIsNotAPoint      = 1,
    SceneObjectDoesNotExist     = 2,
    NonPositiveDimensions       = 3,
  };

  void init_cylinder_from_curve(const CurveHandle& handle, CylinderPatchSurfaceStarter starter, eray::math::Vec2u dim);
  void init_from_starter(const PatchSurfaceStarter& starter, eray::math::Vec2u dim);
  std::expected<void, InitError> init_from_points(eray::math::Vec2u points_dim,
                                                  const std::vector<SceneObjectHandle>& points);

  void update();
  void on_delete();
  bool can_be_deleted() const { return true; }

  void insert_row_top();
  void insert_row_bottom();
  void insert_column_left();
  void insert_column_right();

  void delete_row_top();
  void delete_row_bottom();
  void delete_column_left();
  void delete_column_right();

  void clone_to(PatchSurface& obj) const;

  const eray::math::Vec2u& dimensions() const { return dim_; }
  eray::math::Vec2u control_points_dim() const;

  int tess_level() const { return tess_level_; }
  void set_tess_level(int tesselation);

  /**
   * @brief Returns a matrix representing a frame (Frenet basis).
   *
   * @param t
   * @return eray::math::Mat4f
   */
  [[nodiscard]] eray::math::Mat4f frenet_frame(float u, float v);

  [[nodiscard]] eray::math::Vec3f evaluate(float u, float v);

  [[nodiscard]] std::pair<eray::math::Vec3f, eray::math::Vec3f> evaluate_derivatives(float u, float v);

  [[nodiscard]] std::pair<eray::math::Vec3f, eray::math::Vec3f> aabb_bounding_box();

 private:
  void mark_bezier3_dirty() { bezier_dirty_ = true; }
  void clear();

  std::pair<eray::math::Vec2f, eray::math::Vec2u> find_bezier3_patch_and_param(float u, float v);

 private:
  friend SceneObject;
  friend Point;
  friend BezierPatches;
  friend BPatches;
  friend FillInSurface;

  eray::math::Vec2u dim_;
  int tess_level_ = kDefaultTessLevel;
  std::vector<eray::math::Vec3f> bezier3_points_;  // row-major packed patches
  bool bezier_dirty_ = true;

  std::unordered_set<FillInSurfaceHandle> fill_in_surfaces_;
};

static_assert(CParametricSurfaceObject<PatchSurface>);

}  // namespace mini
