#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/gl/curves_renderer.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/gl/patch_surface_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/gl/scene_objects_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini::gl {

class OpenGLSceneRenderer;

class OpenGLSceneRenderer final : public ISceneRenderer {
 public:
  OpenGLSceneRenderer()        = delete;
  ~OpenGLSceneRenderer() final = default;

  enum class SceneRendererCreationError : uint8_t {
    AssetsLoadingFailed         = 0,
    ShaderProgramCreationFailed = 1,
  };

  static std::expected<std::unique_ptr<ISceneRenderer>, SceneRendererCreationError> create(
      const std::filesystem::path& assets_path, eray::math::Vec2i win_size);

  void push_object_rs_cmd(const SceneObjectRSCommand& cmd) final;
  std::optional<::mini::SceneObjectRS> object_rs(const SceneObjectHandle& handle) final;
  void set_object_rs(const SceneObjectHandle& handle, const ::mini::SceneObjectRS& state) final;

  void push_object_rs_cmd(const CurveRSCommand& cmd) final;
  std::optional<::mini::CurveRS> object_rs(const CurveHandle& handle) final;
  void set_object_rs(const CurveHandle& handle, const ::mini::CurveRS& state) final;

  void push_object_rs_cmd(const PatchSurfaceRSCommand& cmd) final;
  std::optional<::mini::PatchSurfaceRS> object_rs(const PatchSurfaceHandle& handle) final;
  void set_object_rs(const PatchSurfaceHandle& handle, const ::mini::PatchSurfaceRS& state) final;

  void add_billboard(zstring_view name, const eray::res::Image& img) final;
  ::mini::BillboardRS& billboard(zstring_view name) final;

  void show_grid(bool show_grid) final { global_rs_.show_grid = show_grid; }
  bool is_grid_shown() const final { return global_rs_.show_grid; }

  void show_polylines(bool show_polylines) final { global_rs_.show_polylines = show_polylines; }
  bool are_polylines_shown() const final { return global_rs_.show_polylines; }

  void show_points(bool show_polylines) final { global_rs_.show_points = show_polylines; }
  bool are_points_shown() const final { return global_rs_.show_points; }

  void resize_viewport(eray::math::Vec2i win_size) final;
  SamplingResult sample_mouse_pick_box(Scene& scene, size_t x, size_t y, size_t width, size_t height) const final;

  void update(Scene& scene) final;
  void render(Camera& camera) final;

 private:
  friend SceneObjectRSCommandHandler;
  friend CurveRSCommandHandler;

  struct Shaders {
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> param;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> grid;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> polyline;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> bezier;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> bezier_surf;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> sprite;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> instanced_sprite;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> helper_points;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> screen_quad;
  } shaders_;

  struct GlobalRS {
    std::unordered_map<zstring_view, BillboardRS> billboards;

    eray::driver::gl::TextureHandle point_txt;
    eray::driver::gl::TextureHandle helper_point_txt;

    bool show_grid;
    bool show_polylines;
    bool show_points;
  } global_rs_;

  CurvesRenderer curve_renderer_;
  SceneObjectsRenderer scene_objs_renderer_;
  PatchSurfaceRenderer patch_surface_renderer_;

  std::unique_ptr<eray::driver::gl::ViewportFramebuffer> framebuffer_;

 private:
  explicit OpenGLSceneRenderer(Shaders&& shaders, GlobalRS&& global_rs, SceneObjectsRenderer&& objs_rs,
                               CurvesRenderer&& curve_objs_rs, PatchSurfaceRenderer&& patch_surface_rs,
                               std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& framebuffer);
};

}  // namespace mini::gl
