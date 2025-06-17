#pragma once
#include <libminicad/scene/patch_surface.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

struct Triangle;

// ---------------------------------------------------------------------------------------------------------------------
// - FillInSurfaceType -------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

struct GregoryPatches {
 public:
  [[nodiscard]] static zstring_view type_name() noexcept { return "Gregory patches"; }
};

template <typename T>
concept CFillInSurfaceType = requires(T t) {
  { T::type_name() } -> std::same_as<zstring_view>;
};

using FillInSurfaceVariant = std::variant<GregoryPatches>;
MINI_VALIDATE_VARIANT_TYPES(FillInSurfaceVariant, CFillInSurfaceType);

// ---------------------------------------------------------------------------------------------------------------------
// - FillInSurface -----------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

class FillInSurface : public ObjectBase<FillInSurface, FillInSurfaceVariant> {
 public:
  FillInSurface() = delete;
  FillInSurface(const FillInSurfaceHandle& handle, Scene& scene);

  ERAY_DEFAULT_MOVE(FillInSurface)
  ERAY_DELETE_COPY(FillInSurface)

  void mark_points_dirty() { points_dirty_ = true; }

  const std::vector<eray::math::Vec3f>& rational_bezier_points();

  std::generator<eray::math::Vec3f> control_grid_points();
  size_t control_grid_points_count() { return kNeighbors * 2 * kPatchPointsCount; }

  std::generator<eray::math::Vec3f> tangent_grid_points();
  size_t tangent_grid_points_count() { return kNeighbors * 2 * 4; }

  static constexpr size_t kNeighbors        = 3U;
  static constexpr size_t kPatchPointsCount = 20U;
  static constexpr int kDefaultTessLevel    = 4;

  using Boundary = std::array<SceneObjectHandle, 4>;
  struct SurfaceNeighbor {
    std::array<Boundary, 2> boundaries;  // row-major, rows = 4, columns = 2
    PatchSurfaceHandle handle;
  };

  struct SurfaceNeighborhood {
    SurfaceNeighborhood() = delete;

    static SurfaceNeighborhood create(SurfaceNeighbor&& n0, SurfaceNeighbor&& n1, SurfaceNeighbor&& n2);

    Triangle get_triangle();

    std::array<SurfaceNeighbor, kNeighbors> neighbors;

   private:
    friend FillInSurface;
    explicit SurfaceNeighborhood(std::array<SurfaceNeighbor, kNeighbors>&& neighbors);
  };

  enum class InitError : uint8_t {
    PointDoesNotExist = 0,
    NotAPoint         = 1,
    PatchDoesNotExist = 2,
    NotABezierPatch   = 3,
    AlreadyFilled     = 4,
  };

  int tess_level() const { return tess_level_; }
  void set_tess_level(int tesselation);

  std::expected<void, InitError> init(SurfaceNeighborhood&& nighborhood);
  void update();
  void on_delete();
  bool can_be_deleted() const { return true; }

  void clone_to(FillInSurface& obj) const;

 private:
  friend SceneObject;

  enum class ReplaceOperationError : uint8_t { NotAPoint = 0 };
  std::expected<void, ReplaceOperationError> replace(const SceneObjectHandle& old_point_handle, SceneObject& new_point);

 private:
  bool points_dirty_ = false;
  int tess_level_    = kDefaultTessLevel;

  SurfaceNeighborhood neighborhood_;
  std::vector<eray::math::Vec3f> rational_bezier_points_;
};

}  // namespace mini
