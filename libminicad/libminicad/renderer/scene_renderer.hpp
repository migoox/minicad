#pragma once

#include <expected>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/camera/camera.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>

namespace mini {

struct SampledHelperPoint {
  PointListObjectHandle pl_handle;
  size_t helper_point_idx;
};

struct SampledSceneObjects {
  std::vector<SceneObjectHandle> handles;
};

using SamplingResult = std::optional<std::variant<SampledSceneObjects, SampledHelperPoint>>;

/**
 * @brief Rendering API agnostic minicad scene renderer. The implementation of this interface is injected into the
 * Scene on Scene creation.
 *
 */
class ISceneRenderer {
 public:
  virtual ~ISceneRenderer() = default;

  virtual void push_object_rs_cmd(const SceneObjectRSCommand& cmd)                        = 0;
  virtual std::optional<SceneObjectRS> object_rs(const SceneObjectHandle& handle)         = 0;
  virtual void set_object_rs(const SceneObjectHandle& handle, const SceneObjectRS& state) = 0;

  virtual void push_object_rs_cmd(const PointListObjectRSCommand& cmd)                            = 0;
  virtual std::optional<PointListObjectRS> object_rs(const PointListObjectHandle& handle)         = 0;
  virtual void set_object_rs(const PointListObjectHandle& handle, const PointListObjectRS& state) = 0;

  virtual void add_billboard(zstring_view name, const eray::res::Image& img) = 0;
  virtual BillboardRS& billboard(zstring_view name)                          = 0;

  virtual void show_grid(bool show_grid) = 0;
  virtual bool is_grid_shown() const     = 0;

  virtual void resize_viewport(eray::math::Vec2i win_size)                                                          = 0;
  virtual SamplingResult sample_mouse_pick_box(Scene& scene, size_t x, size_t y, size_t width, size_t height) const = 0;

  virtual void update(Scene& scene) = 0;

  virtual void render(Camera& camera) = 0;
};

}  // namespace mini
