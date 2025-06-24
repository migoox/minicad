#pragma once

#include <expected>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/zstring_view.hpp>
#include <libminicad/camera/camera.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/handles.hpp>

#include "liberay/util/object_handle.hpp"

namespace mini {

struct SampledHelperPoint {
  CurveHandle c_handle;
  size_t helper_point_idx;
};

struct SampledSceneObjects {
  std::vector<SceneObjectHandle> handles;
};

struct RendererColors {
  static constexpr auto kNetColor       = eray::math::Vec4f(1.F, 0.59F, 0.4F, 1.F);
  static constexpr auto kPolylinesColor = eray::math::Vec4f(0.843F, 0.894F, 0.949F, 1.F);
  static constexpr auto kVectors        = eray::math::Vec4f(0.62F, 0.867F, 1.F, 1.F);
};

struct Texture {
  size_t width;
  size_t height;
};
using TextureHandle = eray::util::Handle<Texture>;

using SamplingResult = std::optional<std::variant<SampledSceneObjects, SampledHelperPoint>>;

/**
 * @brief Rendering API agnostic minicad scene renderer. The implementation of this interface is injected into the
 * Scene upon Scene creation.
 *
 */
class ISceneRenderer {
 public:
  virtual ~ISceneRenderer() = default;

  virtual void push_object_rs_cmd(const RSCommand& cmd) = 0;

  virtual std::optional<ObjectRS> object_rs(const ObjectHandle& handle)         = 0;
  virtual void set_object_rs(const ObjectHandle& handle, const ObjectRS& state) = 0;

  virtual void add_billboard(zstring_view name, const eray::res::Image& img) = 0;
  virtual BillboardRS& billboard(zstring_view name)                          = 0;

  virtual void show_grid(bool show_grid) = 0;
  virtual bool is_grid_shown() const     = 0;

  virtual void show_polylines(bool show_polylines) = 0;
  virtual bool are_polylines_shown() const         = 0;

  virtual void show_points(bool show_polylines) = 0;
  virtual bool are_points_shown() const         = 0;

  virtual void resize_viewport(eray::math::Vec2i win_size)                                                          = 0;
  virtual SamplingResult sample_mouse_pick_box(Scene& scene, size_t x, size_t y, size_t width, size_t height) const = 0;

  virtual void set_anaglyph_rendering_enabled(bool anaglyph)                            = 0;
  virtual bool is_anaglyph_rendering_enabled() const                                    = 0;
  virtual eray::math::Vec3f anaglyph_output_color_coeffs() const                        = 0;
  virtual void set_anaglyph_output_color_coeffs(const eray::math::Vec3f& output_coeffs) = 0;

  virtual TextureHandle upload_texture(const std::vector<uint32_t>& texture, size_t size_x, size_t size_y) = 0;
  virtual void reupload_texture(const TextureHandle& handle, const std::vector<uint32_t>& texture, size_t size_x,
                                size_t size_y)                                                             = 0;
  virtual void delete_texture(const TextureHandle& texture)                                                = 0;
  virtual std::optional<Texture> get_texture_info(const TextureHandle& texture)                            = 0;
  virtual void draw_imgui_texture_image(const TextureHandle& texture, size_t size_x, size_t size_y)        = 0;

  virtual void update(Scene& scene) = 0;

  virtual void render(const Camera& camera) = 0;

  virtual void debug_point(const eray::math::Vec3f& pos)                                = 0;
  virtual void debug_line(const eray::math::Vec3f& start, const eray::math::Vec3f& end) = 0;
  virtual void clear_debug()                                                            = 0;

  virtual void clear() = 0;
};

}  // namespace mini
