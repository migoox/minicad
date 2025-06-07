#pragma once
#include <libminicad/scene/scene_object.hpp>

#include "liberay/math/vec_fwd.hpp"

namespace mini {

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

  static constexpr size_t kNeighbors        = 3U;
  static constexpr size_t kPatchPointsCount = 20U;
  static constexpr int kDefaultTessLevel    = 4;

  using Boundary = std::array<SceneObjectHandle, 4>;
  struct SurfaceNeighbor {
    std::array<Boundary, 2> boundaries;  // row-major, rows = 4, columns = 2
    PatchSurfaceHandle handle;
  };

  enum class InitError : uint8_t {
    PointDoesNotExist = 0,
    NotAPoint         = 1,
    PatchDoesNotExist = 2,
    NotABezierPatch   = 3,
  };

  int tess_level() const { return tess_level_; }
  void set_tess_level(int tesselation);

  std::expected<void, InitError> init(std::array<SurfaceNeighbor, kNeighbors>&& neighbors);
  void update();
  void on_delete();
  bool can_be_deleted() const { return true; }

 private:
  bool points_dirty_ = false;
  int tess_level_    = kDefaultTessLevel;

  std::array<SurfaceNeighbor, kNeighbors> neighbors_;
  std::vector<eray::math::Vec3f> rational_bezier_points_;
};

}  // namespace mini
