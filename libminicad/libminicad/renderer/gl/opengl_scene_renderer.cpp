#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/gl/scene_objects_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <optional>

#include "libminicad/renderer/gl/patch_surface_renderer.hpp"

namespace mini::gl {

namespace driver = eray::driver;
namespace gl     = eray::driver::gl;
namespace res    = eray::res;
namespace math   = eray::math;

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

  auto shaders = Shaders{
      .param            = std::move(param_prog),                      //
      .grid             = std::move(grid_prog),                       //
      .polyline         = std::move(polyline_prog),                   //
      .bezier           = std::move(bezier_prog),                     //
      .sprite           = std::move(sprite_prog),                     //
      .instanced_sprite = std::move(instanced_sprite_prog),           //
      .helper_points    = std::move(instanced_no_state_sprite_prog),  //
      .screen_quad      = std::move(screen_quad_prog),                //
  };

  auto global_rs = GlobalRS{
      .billboards       = {},                                //
      .point_txt        = create_texture(point_img),         //
      .helper_point_txt = create_texture(helper_point_img),  //
      .show_grid        = true,                              //
  };

  return std::unique_ptr<ISceneRenderer>(new OpenGLSceneRenderer(
      std::move(shaders), std::move(global_rs), SceneObjectsRenderer::create(), CurvesRenderer::create(),
      PatchSurfaceRenderer::create(), std::make_unique<gl::ViewportFramebuffer>(win_size.x, win_size.y)));
}

OpenGLSceneRenderer::OpenGLSceneRenderer(Shaders&& shaders, GlobalRS&& global_rs, SceneObjectsRenderer&& objs_rs,
                                         CurvesRenderer&& curve_objs_rs, PatchSurfaceRenderer&& patch_surface_rs,
                                         std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& framebuffer)
    : shaders_(std::move(shaders)),
      global_rs_(std::move(global_rs)),
      curve_renderer_(std::move(curve_objs_rs)),
      scene_objs_renderer_(std::move(objs_rs)),
      patch_surface_renderer_(std::move(patch_surface_rs)),
      framebuffer_(std::move(framebuffer)) {}

void OpenGLSceneRenderer::push_object_rs_cmd(const SceneObjectRSCommand& cmd) { scene_objs_renderer_.push_cmd(cmd); }

std::optional<::mini::SceneObjectRS> OpenGLSceneRenderer::object_rs(const SceneObjectHandle& handle) {
  return scene_objs_renderer_.object_rs(handle);
}

void OpenGLSceneRenderer::set_object_rs(const SceneObjectHandle& handle, const ::mini::SceneObjectRS& state) {
  scene_objs_renderer_.set_object_rs(handle, state);
}

std::optional<::mini::CurveRS> OpenGLSceneRenderer::object_rs(const CurveHandle& handle) {
  return curve_renderer_.object_rs(handle);
}

void OpenGLSceneRenderer::set_object_rs(const CurveHandle& handle, const ::mini::CurveRS& state) {
  curve_renderer_.set_object_rs(handle, state);
}

void OpenGLSceneRenderer::push_object_rs_cmd(const CurveRSCommand& cmd) { curve_renderer_.push_cmd(cmd); }

void OpenGLSceneRenderer::add_billboard(zstring_view name, const eray::res::Image& img) {
  auto txt = create_texture(img);
  global_rs_.billboards.insert({name, BillboardRS(std::move(txt))});
}

void OpenGLSceneRenderer::push_object_rs_cmd(const PatchSurfaceRSCommand& cmd) {
  patch_surface_renderer_.push_cmd(cmd);
}

std::optional<::mini::PatchSurfaceRS> OpenGLSceneRenderer::object_rs(const PatchSurfaceHandle& handle) {
  return patch_surface_renderer_.object_rs(handle);
}

void OpenGLSceneRenderer::set_object_rs(const PatchSurfaceHandle& handle, const ::mini::PatchSurfaceRS& state) {
  patch_surface_renderer_.set_object_rs(handle, state);
}

::mini::BillboardRS& OpenGLSceneRenderer::billboard(zstring_view name) { return global_rs_.billboards.at(name).state; }

void OpenGLSceneRenderer::resize_viewport(eray::math::Vec2i win_size) {
  framebuffer_->resize(static_cast<size_t>(win_size.x), static_cast<size_t>(win_size.y));
}

void OpenGLSceneRenderer::update(Scene& scene) {
  scene_objs_renderer_.update(scene);
  curve_renderer_.update(scene);
  patch_surface_renderer_.update(scene);
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

void OpenGLSceneRenderer::render(Camera& camera) {
  framebuffer_->bind();
  framebuffer_->clear_pick_render();

  // Prepare the framebuffer
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
  ERAY_GL_CALL(glEnable(GL_DEPTH_TEST));
  ERAY_GL_CALL(glEnable(GL_BLEND));
  ERAY_GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  ERAY_GL_CALL(glClearColor(0.09, 0.05, 0.09, 1.F));
  ERAY_GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  // Render parameterized surfaces
  framebuffer_->begin_pick_render();
  ERAY_GL_CALL(glPatchParameteri(GL_PATCH_VERTICES, 4));
  shaders_.param->set_uniform("u_vMat", camera.view_matrix());
  shaders_.param->set_uniform("u_pMat", camera.proj_matrix());
  shaders_.param->bind();
  shaders_.param->set_uniform("u_fill", true);
  scene_objs_renderer_.render_parameterized_surfaces_filled();
  framebuffer_->end_pick_render();
  shaders_.param->set_uniform("u_fill", false);
  scene_objs_renderer_.render_parameterized_surfaces();

  // Render polylines and control grids
  shaders_.polyline->bind();
  shaders_.polyline->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  curve_renderer_.render_polylines();
  patch_surface_renderer_.render_control_grids();

  // Render Curves
  shaders_.bezier->bind();
  shaders_.bezier->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  shaders_.bezier->set_uniform("u_width", static_cast<float>(framebuffer_->width()));
  shaders_.bezier->set_uniform("u_height", static_cast<float>(framebuffer_->height()));
  shaders_.bezier->set_uniform("u_color", math::Vec4f(1.F, 0.59F, 0.4F, 1.F));
  curve_renderer_.render_curves();

  // Render grid
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  if (global_rs_.show_grid) {
    shaders_.grid->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
    shaders_.grid->set_uniform("u_vInvMat", camera.inverse_view_matrix());
    shaders_.grid->set_uniform("u_camWorldPos", camera.transform.pos());
    shaders_.grid->bind();
    ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  }

  ERAY_GL_CALL(glClear(GL_DEPTH_BUFFER_BIT));
  ERAY_GL_CALL(glEnable(GL_DEPTH_TEST));

  // Render Bernstein control points for B-Splines
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  framebuffer_->begin_pick_render();
  ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, global_rs_.helper_point_txt.get()));
  shaders_.helper_points->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  shaders_.helper_points->set_uniform("u_scale", 0.02F);
  shaders_.helper_points->set_uniform("u_aspectRatio", camera.aspect_ratio());
  shaders_.helper_points->set_uniform("u_textureSampler", 0);
  shaders_.helper_points->bind();
  curve_renderer_.render_helper_points();
  framebuffer_->end_pick_render();

  // Render control points
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  framebuffer_->begin_pick_render();
  ERAY_GL_CALL(glActiveTexture(GL_TEXTURE0));
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, global_rs_.point_txt.get()));
  shaders_.instanced_sprite->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  shaders_.instanced_sprite->set_uniform("u_scale", 0.03F);
  shaders_.instanced_sprite->set_uniform("u_aspectRatio", camera.aspect_ratio());
  shaders_.instanced_sprite->set_uniform("u_textureSampler", 0);
  shaders_.instanced_sprite->bind();
  scene_objs_renderer_.render_control_points();
  framebuffer_->end_pick_render();

  // Render billboards
  ERAY_GL_CALL(glDisable(GL_DEPTH_TEST));
  for (auto& [name, billboard] : global_rs_.billboards) {
    if (!billboard.state.show) {
      continue;
    }

    ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, billboard.texture.get()));
    shaders_.sprite->set_uniform("u_worldPos", billboard.state.position);
    shaders_.sprite->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
    shaders_.sprite->set_uniform("u_aspectRatio", camera.aspect_ratio());
    shaders_.sprite->set_uniform("u_scale", billboard.state.scale);
    shaders_.sprite->set_uniform("u_textureSampler", 0);
    shaders_.sprite->bind();
    ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
  }
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

  // Render to the default framebuffer
  ERAY_GL_CALL(glDisable(GL_DEPTH_TEST));
  ERAY_GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  ERAY_GL_CALL(glBindTexture(GL_TEXTURE_2D, framebuffer_->color_texture()));
  ERAY_GL_CALL(glClearColor(1.0F, 1.0F, 1.0F, 1.0F));
  ERAY_GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
  shaders_.screen_quad->bind();
  shaders_.screen_quad->set_uniform("u_textureSampler", 0);
  ERAY_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, 6));
}

}  // namespace mini::gl
