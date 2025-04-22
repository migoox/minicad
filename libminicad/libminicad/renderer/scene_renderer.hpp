#pragma once

#include <expected>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/camera/camera.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini {

/**
 * @brief Rendering API agnostic minicad scene renderer.
 *
 */
class ISceneRenderer {
 public:
  virtual ~ISceneRenderer() = default;

  virtual void push_object_rs_cmd(const SceneObjectRSCommand& obj)                        = 0;
  virtual std::optional<SceneObjectRS> object_rs(const SceneObjectHandle& handle)         = 0;
  virtual void set_object_rs(const SceneObjectHandle& handle, const SceneObjectRS& state) = 0;

  virtual void push_object_rs_cmd(const PointListObjectRSCommand& obj)                            = 0;
  virtual std::optional<PointListObjectRS> object_rs(const PointListObjectHandle& handle)         = 0;
  virtual void set_object_rs(const PointListObjectHandle& handle, const PointListObjectRS& state) = 0;

  virtual void add_billboard(zstring_view name, const eray::res::Image& img) = 0;
  virtual BillboardRS& billboard(zstring_view name)                          = 0;

  virtual void show_grid(bool show_grid) = 0;
  virtual bool is_grid_shown() const     = 0;

  virtual void resize_viewport(eray::math::Vec2i win_size)                                                     = 0;
  virtual std::unordered_set<int> sample_mouse_pick_box(size_t x, size_t y, size_t width, size_t height) const = 0;

  virtual void update(Scene& scene) = 0;

  virtual void render(Scene& scene, Camera& camera) = 0;
};

}  // namespace mini
