#pragma once

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <libminicad/renderer/gl/approx_curve_renderer.hpp>
#include <libminicad/renderer/gl/curves_renderer.hpp>
#include <libminicad/renderer/gl/fill_in_surfaces_renderer.hpp>
#include <libminicad/renderer/gl/line_buffer.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/gl/patch_surface_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/gl/scene_objects_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <unordered_map>

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

  void push_object_rs_cmd(const RSCommand& cmd) final;
  std::optional<ObjectRS> object_rs(const ObjectHandle& handle) final;
  void set_object_rs(const ObjectHandle& handle, const ObjectRS& state) final;

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

  void set_anaglyph_rendering_enabled(bool anaglyph) final;
  bool is_anaglyph_rendering_enabled() const final;
  eray::math::Vec3f anaglyph_output_color_coeffs() const final { return global_rs_.anaglyph_output_coeffs; }
  void set_anaglyph_output_color_coeffs(const eray::math::Vec3f& output_coeffs) final {
    global_rs_.anaglyph_output_coeffs = output_coeffs;
  }

  TextureHandle upload_texture(const std::vector<uint32_t>& texture, size_t size_x, size_t size_y) final;
  void delete_texture(const TextureHandle& texture) final;
  std::optional<Texture> get_texture_info(const TextureHandle& texture) final;
  void draw_imgui_texture_image(const TextureHandle& texture) final;

  void debug_point(const eray::math::Vec3f& pos) final;
  void debug_line(const eray::math::Vec3f& start, const eray::math::Vec3f& end) final;
  void clear_debug() final;

  void update(Scene& scene) final;
  void render(const Camera& camera) final;
  void clear() final;

 private:
  void render_internal(eray::driver::gl::ViewportFramebuffer& fb, const Camera& camera,
                       const eray::math::Mat4f& view_mat, const eray::math::Mat4f& proj_mat,
                       const eray::math::Vec3f& background_color);

 private:
  friend SceneObjectRSCommandHandler;
  friend CurveRSCommandHandler;

  struct Shaders {
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> param;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> grid;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> polyline;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> lines;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> bezier;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> bezier_surf;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> rational_bezier_surf;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> sprite;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> instanced_sprite;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> helper_points;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> screen_quad;
    std::unique_ptr<eray::driver::gl::RenderingShaderProgram> anaglyph_merger;
  } shaders_;

  struct GlobalRS {
    std::unordered_map<zstring_view, BillboardRS> billboards;

    std::vector<eray::math::Vec3f> debug_points;
    PointsVAO debug_lines;

    std::unordered_map<TextureHandle, std::pair<eray::driver::gl::TextureHandle, Texture>> textures;

    eray::driver::gl::TextureHandle point_txt;
    eray::driver::gl::TextureHandle helper_point_txt;

    bool show_grid;
    bool show_polylines;
    bool show_points;
    bool anaglyph_enabled;

    eray::math::Vec3f anaglyph_output_coeffs;

    uint32_t timestamp = 0;
    uint32_t signature = 0;
  } global_rs_;

  CurvesRenderer curve_renderer_;
  SceneObjectsRenderer scene_objs_renderer_;
  PatchSurfaceRenderer patch_surface_renderer_;
  FillInSurfaceRenderer fill_in_surface_renderer_;
  ApproxCurvesRenderer intersection_curves_renderer_;

  std::unique_ptr<eray::driver::gl::ViewportFramebuffer> framebuffer_;
  std::unique_ptr<eray::driver::gl::ViewportFramebuffer> right_eye_framebuffer_;

 private:
  explicit OpenGLSceneRenderer(Shaders&& shaders, GlobalRS&& global_rs, SceneObjectsRenderer&& objs_rs,
                               CurvesRenderer&& curve_objs_rs, PatchSurfaceRenderer&& patch_surface_rs,
                               FillInSurfaceRenderer&& fill_in_surface_rs,
                               ApproxCurvesRenderer&& intersection_curves_rs,
                               std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& framebuffer,
                               std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& right_framebuffer);
};

}  // namespace mini::gl
