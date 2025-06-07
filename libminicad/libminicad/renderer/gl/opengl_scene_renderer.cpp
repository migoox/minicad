#include <glad/gl.h>
#include <imgui/imgui.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/curves_renderer.hpp>
#include <libminicad/renderer/gl/fill_in_surfaces_renderer.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/gl/patch_surface_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/gl/scene_objects_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <optional>
#include <variant>

namespace mini::gl {

namespace driver = eray::driver;
namespace gl     = eray::driver::gl;
namespace res    = eray::res;
namespace math   = eray::math;
namespace util   = eray::util;

namespace {

gl::TextureHandle create_texture(const res::Image& image) {
  GLuint texture = 0;
  ERAY_GL_CALL(glGenTextures(1, &texture));
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, texture));
  ERAY_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  ERAY_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  ERAY_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  ERAY_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(image.width()), static_cast<GLsizei>(image.height()), 0,
               GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, image.raw());
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

  return gl::TextureHandle(texture);
}

}  // namespace

std::expected<std::unique_ptr<ISceneRenderer>, OpenGLSceneRenderer::SceneRendererCreationError>
OpenGLSceneRenderer::create(const std::filesystem::path& assets_path, eray::math::Vec2i win_size) {
  auto manager = driver::GLSLShaderManager();

  // NOLINTBEGIN
#define TRY_UNWRAP_ASSET(var, expr) \
  TRY_UNWRAP_DEFINE_TRANSFORM_ERR(var, expr, SceneRendererCreationError::AssetsLoadingFailed)

#define TRY_UNWRAP_PROGRAM(var, expr) \
  TRY_UNWRAP_DEFINE_TRANSFORM_ERR(var, expr, SceneRendererCreationError::ShaderProgramCreationFailed)
  // NOLINTEND

  auto shaders_path = assets_path / "shaders";
  TRY_UNWRAP_ASSET(param_vert, manager.load_shader(shaders_path / "parametric_surfaces" / "param.vert"));
  TRY_UNWRAP_ASSET(param_frag, manager.load_shader(shaders_path / "parametric_surfaces" / "param.frag"));
  TRY_UNWRAP_ASSET(param_tesc, manager.load_shader(shaders_path / "parametric_surfaces" / "param.tesc"));
  TRY_UNWRAP_ASSET(param_tese, manager.load_shader(shaders_path / "parametric_surfaces" / "param.tese"));
  TRY_UNWRAP_PROGRAM(param_prog,
                     gl::RenderingShaderProgram::create("param_shader", std::move(param_vert), std::move(param_frag),
                                                        std::move(param_tesc), std::move(param_tese)));

  TRY_UNWRAP_ASSET(grid_vert, manager.load_shader(shaders_path / "utils" / "grid.vert"));
  TRY_UNWRAP_ASSET(grid_frag, manager.load_shader(shaders_path / "utils" / "grid.frag"));
  TRY_UNWRAP_PROGRAM(grid_prog,
                     gl::RenderingShaderProgram::create("grid_shader", std::move(grid_vert), std::move(grid_frag)));

  TRY_UNWRAP_ASSET(polyline_vert, manager.load_shader(shaders_path / "curves" / "polyline.vert"));
  TRY_UNWRAP_ASSET(polyline_frag, manager.load_shader(shaders_path / "utils" / "solid_color.frag"));
  TRY_UNWRAP_PROGRAM(polyline_prog, gl::RenderingShaderProgram::create("polyline_shader", std::move(polyline_vert),
                                                                       std::move(polyline_frag)));

  TRY_UNWRAP_ASSET(bezier_vert, manager.load_shader(shaders_path / "curves" / "curve.vert"));
  TRY_UNWRAP_ASSET(bezier_tesc, manager.load_shader(shaders_path / "curves" / "bezier.tesc"));
  TRY_UNWRAP_ASSET(bezier_tese, manager.load_shader(shaders_path / "curves" / "bezier.tese"));
  TRY_UNWRAP_ASSET(bezier_frag, manager.load_shader(shaders_path / "utils" / "solid_color.frag"));
  TRY_UNWRAP_PROGRAM(bezier_prog,
                     gl::RenderingShaderProgram::create("bezier_shader", std::move(bezier_vert), std::move(bezier_frag),
                                                        std::move(bezier_tesc), std::move(bezier_tese)));

  TRY_UNWRAP_ASSET(bezier_surf_vert, manager.load_shader(shaders_path / "patch_surfaces" / "surface.vert"));
  TRY_UNWRAP_ASSET(bezier_surf_tesc, manager.load_shader(shaders_path / "patch_surfaces" / "bezier.tesc"));
  TRY_UNWRAP_ASSET(bezier_surf_tese, manager.load_shader(shaders_path / "patch_surfaces" / "bezier.tese"));
  TRY_UNWRAP_ASSET(bezier_surf_frag, manager.load_shader(shaders_path / "utils" / "solid_color.frag"));
  TRY_UNWRAP_PROGRAM(bezier_surf_prog, gl::RenderingShaderProgram::create(
                                           "bezier_shader", std::move(bezier_surf_vert), std::move(bezier_surf_frag),
                                           std::move(bezier_surf_tesc), std::move(bezier_surf_tese)));

  TRY_UNWRAP_ASSET(rational_bezier_surf_vert, manager.load_shader(shaders_path / "patch_surfaces" / "surface.vert"));
  TRY_UNWRAP_ASSET(rational_bezier_surf_tesc,
                   manager.load_shader(shaders_path / "patch_surfaces" / "rational_bezier.tesc"));
  TRY_UNWRAP_ASSET(rational_bezier_surf_tese,
                   manager.load_shader(shaders_path / "patch_surfaces" / "rational_bezier.tese"));
  TRY_UNWRAP_ASSET(rational_bezier_surf_frag, manager.load_shader(shaders_path / "utils" / "solid_color.frag"));
  TRY_UNWRAP_PROGRAM(rational_bezier_surf_prog,
                     gl::RenderingShaderProgram::create(
                         "bezier_shader", std::move(rational_bezier_surf_vert), std::move(rational_bezier_surf_frag),
                         std::move(rational_bezier_surf_tesc), std::move(rational_bezier_surf_tese)));

  TRY_UNWRAP_ASSET(sprite_vert, manager.load_shader(shaders_path / "sprites" / "sprite.vert"));
  TRY_UNWRAP_ASSET(sprite_frag, manager.load_shader(shaders_path / "sprites" / "sprite.frag"));
  TRY_UNWRAP_PROGRAM(
      sprite_prog, gl::RenderingShaderProgram::create("sprite_shader", std::move(sprite_vert), std::move(sprite_frag)));

  TRY_UNWRAP_ASSET(instanced_sprite_vert, manager.load_shader(shaders_path / "sprites" / "sprite_instanced.vert"));
  TRY_UNWRAP_ASSET(instanced_sprite_frag, manager.load_shader(shaders_path / "sprites" / "sprite_instanced.frag"));
  TRY_UNWRAP_ASSET(instanced_sprite_geom, manager.load_shader(shaders_path / "sprites" / "sprite_instanced.geom"));
  TRY_UNWRAP_PROGRAM(instanced_sprite_prog,
                     gl::RenderingShaderProgram::create("instanced_sprite_shader", std::move(instanced_sprite_vert),
                                                        std::move(instanced_sprite_frag), std::nullopt, std::nullopt,
                                                        std::move(instanced_sprite_geom)));

  TRY_UNWRAP_ASSET(instanced_no_state_sprite_vert,
                   manager.load_shader(shaders_path / "sprites" / "helper_points.vert"));
  TRY_UNWRAP_ASSET(instanced_no_state_sprite_frag,
                   manager.load_shader(shaders_path / "sprites" / "sprite_instanced.frag"));
  TRY_UNWRAP_ASSET(instanced_no_state_sprite_geom,
                   manager.load_shader(shaders_path / "sprites" / "sprite_instanced.geom"));
  TRY_UNWRAP_PROGRAM(
      instanced_no_state_sprite_prog,
      gl::RenderingShaderProgram::create("points_helper_shader", std::move(instanced_no_state_sprite_vert),
                                         std::move(instanced_no_state_sprite_frag), std::nullopt, std::nullopt,
                                         std::move(instanced_no_state_sprite_geom)));

  TRY_UNWRAP_ASSET(point_img, eray::res::Image::load_from_path(assets_path / "img" / "point.png"));
  TRY_UNWRAP_ASSET(helper_point_img, eray::res::Image::load_from_path(assets_path / "img" / "helper_point.png"));

  TRY_UNWRAP_ASSET(screen_quad_vert, manager.load_shader(shaders_path / "utils" / "screen_quad.vert"));
  TRY_UNWRAP_ASSET(screen_quad_frag, manager.load_shader(shaders_path / "utils" / "screen_quad.frag"));
  TRY_UNWRAP_PROGRAM(screen_quad_prog,
                     gl::RenderingShaderProgram::create("screen_quad_shader", std::move(screen_quad_vert),
                                                        std::move(screen_quad_frag)));

  TRY_UNWRAP_ASSET(anaglyph_merger_vert, manager.load_shader(shaders_path / "utils" / "screen_quad.vert"));
  TRY_UNWRAP_ASSET(anaglyph_merger_frag, manager.load_shader(shaders_path / "utils" / "anaglyph_merger.frag"));
  TRY_UNWRAP_PROGRAM(anaglyph_merger_prog,
                     gl::RenderingShaderProgram::create("anaglyph_merger_shader", std::move(anaglyph_merger_vert),
                                                        std::move(anaglyph_merger_frag)));

  auto shaders = Shaders{
      .param                = std::move(param_prog),                      //
      .grid                 = std::move(grid_prog),                       //
      .polyline             = std::move(polyline_prog),                   //
      .bezier               = std::move(bezier_prog),                     //
      .bezier_surf          = std::move(bezier_surf_prog),                //
      .rational_bezier_surf = std::move(rational_bezier_surf_prog),       //
      .sprite               = std::move(sprite_prog),                     //
      .instanced_sprite     = std::move(instanced_sprite_prog),           //
      .helper_points        = std::move(instanced_no_state_sprite_prog),  //
      .screen_quad          = std::move(screen_quad_prog),                //
      .anaglyph_merger      = std::move(anaglyph_merger_prog)             //
  };

  auto global_rs = GlobalRS{
      .billboards             = {},                                //
      .point_txt              = create_texture(point_img),         //
      .helper_point_txt       = create_texture(helper_point_img),  //
      .show_grid              = true,                              //
      .show_polylines         = true,                              //
      .show_points            = true,                              //
      .anaglyph_enabled       = false,                             //
      .anaglyph_output_coeffs = eray::math::Vec3f::filled(0.F),
  };

  return std::unique_ptr<ISceneRenderer>(
      new OpenGLSceneRenderer(std::move(shaders), std::move(global_rs), SceneObjectsRenderer::create(),
                              CurvesRenderer::create(), PatchSurfaceRenderer::create(), FillInSurfaceRenderer::create(),
                              std::make_unique<gl::ViewportFramebuffer>(win_size.x, win_size.y),
                              std::make_unique<gl::ViewportFramebuffer>(win_size.x, win_size.y)));
}

OpenGLSceneRenderer::OpenGLSceneRenderer(Shaders&& shaders, GlobalRS&& global_rs, SceneObjectsRenderer&& objs_rs,
                                         CurvesRenderer&& curve_objs_rs, PatchSurfaceRenderer&& patch_surface_rs,
                                         FillInSurfaceRenderer&& fill_in_surface_rs,
                                         std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& framebuffer,
                                         std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& right_framebuffer)
    : shaders_(std::move(shaders)),
      global_rs_(std::move(global_rs)),
      curve_renderer_(std::move(curve_objs_rs)),
      scene_objs_renderer_(std::move(objs_rs)),
      patch_surface_renderer_(std::move(patch_surface_rs)),
      fill_in_surface_renderer_(std::move(fill_in_surface_rs)),
      framebuffer_(std::move(framebuffer)),
      right_eye_framebuffer_(std::move(right_framebuffer)) {}

void OpenGLSceneRenderer::push_object_rs_cmd(const RSCommand& cmd) {
  std::visit(eray::util::match{
                 [this](const SceneObjectRSCommand& v) { scene_objs_renderer_.push_cmd(v); },
                 [this](const CurveRSCommand& v) { curve_renderer_.push_cmd(v); },
                 [this](const PatchSurfaceRSCommand& v) { patch_surface_renderer_.push_cmd(v); },
                 [this](const FillInSurfaceRSCommand& v) { fill_in_surface_renderer_.push_cmd(v); },
             },
             cmd);
}
std::optional<ObjectRS> OpenGLSceneRenderer::object_rs(const ObjectHandle& handle) {
  return std::visit(
      eray::util::match{
          [this](const SceneObjectHandle& handle) -> std::optional<ObjectRS> {
            return scene_objs_renderer_.object_rs(handle);
          },
          [this](const CurveHandle& handle) -> std::optional<ObjectRS> { return curve_renderer_.object_rs(handle); },
          [this](const PatchSurfaceHandle& handle) -> std::optional<ObjectRS> {
            return patch_surface_renderer_.object_rs(handle);
          },
          [this](const FillInSurfaceHandle& handle) -> std::optional<ObjectRS> {
            return fill_in_surface_renderer_.object_rs(handle);
          },
      },
      handle);
}

void OpenGLSceneRenderer::set_object_rs(const ObjectHandle& handle, const ObjectRS& state) {
  std::visit(
      eray::util::match{
          [&](const SceneObjectHandle& h, const SceneObjectRS& s) { scene_objs_renderer_.set_object_rs(h, s); },
          [&](const CurveHandle& h, const CurveRS& s) { curve_renderer_.set_object_rs(h, s); },
          [&](const PatchSurfaceHandle& h, const PatchSurfaceRS& s) { patch_surface_renderer_.set_object_rs(h, s); },
          [&](const FillInSurfaceHandle& h, const FillInSurfaceRS& s) {
            fill_in_surface_renderer_.set_object_rs(h, s);
          },
          [&](auto&&, auto&&) { util::Logger::warn("Detected handle and rendering state mismatch."); }},
      handle, state);
}

void OpenGLSceneRenderer::add_billboard(zstring_view name, const eray::res::Image& img) {
  auto txt = create_texture(img);
  global_rs_.billboards.insert({name, BillboardRS(std::move(txt))});
}
void OpenGLSceneRenderer::set_anaglyph_rendering_enabled(bool anaglyph) { global_rs_.anaglyph_enabled = anaglyph; }

bool OpenGLSceneRenderer::is_anaglyph_rendering_enabled() const { return global_rs_.anaglyph_enabled; }

::mini::BillboardRS& OpenGLSceneRenderer::billboard(zstring_view name) { return global_rs_.billboards.at(name).state; }

void OpenGLSceneRenderer::resize_viewport(eray::math::Vec2i win_size) {
  framebuffer_->resize(static_cast<size_t>(win_size.x), static_cast<size_t>(win_size.y));
  right_eye_framebuffer_->resize(static_cast<size_t>(win_size.x), static_cast<size_t>(win_size.y));
}

void OpenGLSceneRenderer::update(Scene& scene) {
  scene_objs_renderer_.update(scene);
  curve_renderer_.update(scene);
  patch_surface_renderer_.update(scene);
  fill_in_surface_renderer_.update(scene);
}

SamplingResult OpenGLSceneRenderer::sample_mouse_pick_box(Scene& scene, size_t x, size_t y, size_t width,
                                                          size_t height) const {
  framebuffer_->bind();
  auto ids = framebuffer_->sample_mouse_pick_box(x, y, width, height);

  if (ids.empty()) {
    return std::nullopt;
  }
  if (ids.size() == 1) {
    auto id = *ids.begin();
    // Check if it's a helper point
    if (id & (1 << 31)) {
      auto helper_point_idx = id & ~(1 << 31);

      if (auto result = this->curve_renderer_.find_helper_point_by_idx(static_cast<size_t>(helper_point_idx))) {
        return SampledHelperPoint{.c_handle = result->first, .helper_point_idx = result->second};
      }
      return std::nullopt;
    }
  }

  auto handles = std::vector<SceneObjectHandle>();
  for (auto id : ids) {
    if (auto h = scene.arena<SceneObject>().handle_by_obj_id(static_cast<std::uint32_t>(id))) {
      if (scene.arena<SceneObject>().exists(*h)) {
        handles.push_back(*h);
      }
    }
  }

  return SampledSceneObjects{.handles = handles};
}

void OpenGLSceneRenderer::render(const Camera& camera) {
  if (!is_anaglyph_rendering_enabled()) {
    render_internal(*framebuffer_, camera, camera.view_matrix(), camera.proj_matrix(),
                    math::Vec3f(0.09F, 0.05F, 0.09F));

    // Render to the default framebuffer
    ERAY_GL_CALL(glDisable(GL_DEPTH_TEST));
    ERAY_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
    ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, framebuffer_->color_texture()));

    ERAY_GL_CALL(glClearColor(1.0F, 1.0F, 1.0F, 1.0F));
    ERAY_GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    shaders_.screen_quad->bind();
    shaders_.screen_quad->set_uniform("u_textureSampler", 0);
    ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  } else {
    render_internal(*framebuffer_, camera, camera.view_matrix(), camera.stereo_left_proj_matrix(),
                    math::Vec3f(0.0F, 0.0F, 0.0F));
    render_internal(*right_eye_framebuffer_, camera, camera.view_matrix(), camera.stereo_right_proj_matrix(),
                    math::Vec3f(0.0F, 0.0F, 0.0F));

    ERAY_GL_CALL(glDisable(GL_DEPTH_TEST));
    ERAY_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
    ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, framebuffer_->color_texture()));

    ERAY_GL_CALL(glActiveTexture(GL_TEXTURE1));
    ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, right_eye_framebuffer_->color_texture()));

    ERAY_GL_CALL(glClearColor(1.0F, 1.0F, 1.0F, 1.0F));
    ERAY_GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
    shaders_.anaglyph_merger->bind();
    shaders_.anaglyph_merger->set_uniform("u_left", 0);
    shaders_.anaglyph_merger->set_uniform("u_right", 1);
    shaders_.anaglyph_merger->set_uniform("u_output_coeffs", global_rs_.anaglyph_output_coeffs);

    ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  }
}

void OpenGLSceneRenderer::render_internal(eray::driver::gl::ViewportFramebuffer& fb, const Camera& camera,
                                          const eray::math::Mat4f& view_mat, const eray::math::Mat4f& proj_mat,
                                          const eray::math::Vec3f& background_color) {
  fb.bind();
  fb.clear_pick_render();

  // Prepare the framebuffer
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
  ERAY_GL_CALL(glEnable(GL_DEPTH_TEST));
  ERAY_GL_CALL(glEnable(GL_BLEND));
  ERAY_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  ERAY_GL_CALL(glClearColor(background_color.x, background_color.y, background_color.z, 1.0));
  ERAY_GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  // Render parameterized surfaces
  fb.begin_pick_render();
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 4));
  shaders_.param->set_uniform("u_vMat", view_mat);
  shaders_.param->set_uniform("u_pMat", proj_mat);
  shaders_.param->bind();
  shaders_.param->set_uniform("u_fill", true);
  scene_objs_renderer_.render_parameterized_surfaces_filled();
  fb.end_pick_render();
  shaders_.param->set_uniform("u_fill", false);
  scene_objs_renderer_.render_parameterized_surfaces();

  // Render polylines and control grids
  if (are_polylines_shown()) {
    shaders_.polyline->bind();
    shaders_.polyline->set_uniform("u_pvMat", proj_mat * view_mat);
    curve_renderer_.render_polylines();
    // patch_surface_renderer_.render_control_grids();
    fill_in_surface_renderer_.render_control_grids();
  }

  // Render Curves
  shaders_.bezier->bind();
  shaders_.bezier->set_uniform("u_pvMat", proj_mat * view_mat);
  shaders_.bezier->set_uniform("u_width", static_cast<float>(fb.width()));
  shaders_.bezier->set_uniform("u_height", static_cast<float>(fb.height()));
  shaders_.bezier->set_uniform("u_color", math::Vec4f(1.F, 0.59F, 0.4F, 1.F));
  curve_renderer_.render_curves();

  // Render Patch Surface
  shaders_.bezier_surf->bind();
  shaders_.bezier_surf->set_uniform("u_pvMat", proj_mat * view_mat);
  shaders_.bezier_surf->set_uniform("u_color", math::Vec4f(1.F, 0.59F, 0.4F, 1.F));
  shaders_.bezier_surf->set_uniform("u_horizontal", true);
  patch_surface_renderer_.render_surfaces();
  shaders_.bezier_surf->set_uniform("u_horizontal", false);
  patch_surface_renderer_.render_surfaces();

  // Render Fill In Surface
  shaders_.rational_bezier_surf->bind();
  shaders_.rational_bezier_surf->set_uniform("u_pvMat", proj_mat * view_mat);
  shaders_.rational_bezier_surf->set_uniform("u_color", math::Vec4f(1.F, 0.59F, 0.4F, 1.F));
  shaders_.rational_bezier_surf->set_uniform("u_horizontal", true);
  fill_in_surface_renderer_.render_fill_in_surfaces();
  shaders_.rational_bezier_surf->set_uniform("u_horizontal", false);
  fill_in_surface_renderer_.render_fill_in_surfaces();

  // Render grid
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  if (global_rs_.show_grid) {
    shaders_.grid->set_uniform("u_pvMat", proj_mat * view_mat);
    shaders_.grid->set_uniform("u_vInvMat", camera.inverse_view_matrix());
    shaders_.grid->set_uniform("u_camWorldPos", camera.transform.pos());
    shaders_.grid->bind();
    ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  }

  ERAY_GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
  ERAY_GL_CALL(glEnable(GL_DEPTH_TEST));

  // Render Bernstein control points for B-Splines
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  fb.begin_pick_render();
  ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, global_rs_.helper_point_txt.get()));
  shaders_.helper_points->set_uniform("u_pvMat", proj_mat * view_mat);
  shaders_.helper_points->set_uniform("u_scale", 0.02F / 2.F);
  shaders_.helper_points->set_uniform("u_aspectRatio", camera.aspect_ratio());
  shaders_.helper_points->set_uniform("u_textureSampler", 0);
  shaders_.helper_points->bind();
  curve_renderer_.render_helper_points();
  fb.end_pick_render();

  // Render control points
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  if (are_points_shown()) {
    fb.begin_pick_render();
    ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
    ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, global_rs_.point_txt.get()));
    shaders_.instanced_sprite->set_uniform("u_pvMat", proj_mat * view_mat);
    shaders_.instanced_sprite->set_uniform("u_scale", 0.03F / 2.F);
    shaders_.instanced_sprite->set_uniform("u_aspectRatio", camera.aspect_ratio());
    shaders_.instanced_sprite->set_uniform("u_textureSampler", 0);
    shaders_.instanced_sprite->bind();
    scene_objs_renderer_.render_control_points();
    fb.end_pick_render();
  }

  // Render billboards
  ERAY_GL_CALL(glDisable(GL_DEPTH_TEST));
  for (auto& [name, billboard] : global_rs_.billboards) {
    if (!billboard.state.show) {
      continue;
    }

    ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, billboard.texture.get()));
    shaders_.sprite->set_uniform("u_worldPos", billboard.state.position);
    shaders_.sprite->set_uniform("u_pvMat", proj_mat * view_mat);
    shaders_.sprite->set_uniform("u_aspectRatio", camera.aspect_ratio());
    shaders_.sprite->set_uniform("u_scale", billboard.state.scale);
    shaders_.sprite->set_uniform("u_textureSampler", 0);
    shaders_.sprite->bind();
    ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  }
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

}  // namespace mini::gl
