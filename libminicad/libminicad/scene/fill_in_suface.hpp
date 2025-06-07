#pragma once
#include <libminicad/scene/scene_object.hpp>

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

  std::generator<eray::math::Vec3f> control_grid_points() const;
  size_t control_grid_points_count() { return kNeighbors * 2 * 20; }

  static constexpr size_t kNeighbors = 3U;

  using Boundary = std::array<SceneObjectHandle, 4>;
  struct SurfaceNeighbor {
    std::array<Boundary, 2> boundaries;  // row-major, rows = 4, columns = 2
    PatchSurfaceHandle handle;
  };

  void init(std::array<SurfaceNeighbor, kNeighbors>&& neighbors);
  void update();
  void on_delete();
  bool can_be_deleted() const { return true; }

 private:
  std::array<SurfaceNeighbor, kNeighbors> neighbors_;
  std::vector<eray::math::Vec3f> rational_bezier_points_;
  std::vector<eray::math::Vec3f> debug_;
};

}  // namespace mini
