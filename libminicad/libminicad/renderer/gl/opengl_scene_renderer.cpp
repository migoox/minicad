#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <variant>

#include "liberay/driver/gl/buffer.hpp"

namespace mini::gl {

namespace driver = eray::driver;
namespace gl     = eray::driver::gl;
namespace util   = eray::util;
namespace res    = eray::res;
namespace math   = eray::math;

void SceneObjectRSCommandHandler::operator()(const SceneObjectRSCommand::Internal::AddObject&) {
  auto& rs = renderer.scene_objs_rs_;

  if (auto obj = scene.get_obj(cmd_ctx.handle)) {
    std::visit(eray::util::match{
                   [&](const Point&) {
                     auto i   = cmd_ctx.handle.obj_id;
                     auto ind = rs.transferred_points_buff.size();
                     rs.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
                     rs.transferred_point_ind.insert({cmd_ctx.handle, ind});
                     rs.transferred_points_buff.push_back(cmd_ctx.handle);
                   },  //
                   [&](const Torus&) {
                     auto ind = rs.transferred_torus_buff.size();
                     rs.transferred_torus_ind.insert({cmd_ctx.handle, ind});
                     rs.transferred_torus_buff.push_back(cmd_ctx.handle);
                   }  //
               },
               obj.value()->object);
  }
}

void SceneObjectRSCommandHandler::operator()(const SceneObjectRSCommand::UpdateObjectMembers&) {
  auto& rs           = renderer.scene_objs_rs_;
  const auto& handle = cmd_ctx.handle;

  if (auto obj = scene.get_obj(handle)) {
    std::visit(util::match{[&](const Point&) {
                             auto p = obj.value()->transform.pos();
                             rs.points_vao.vbo().set_attribute_value(handle.obj_id, "pos", p.raw_ptr());

                             if (rs.scene_objects_rs.contains(handle)) {
                               auto vs = static_cast<int>(rs.scene_objects_rs.at(handle).state.visibility);
                               rs.points_vao.vbo().set_attribute_value(handle.obj_id, "state", &vs);
                             } else {
                               auto vs = 0;
                               rs.points_vao.vbo().set_attribute_value(handle.obj_id, "state", &vs);
                             }
                           },
                           [&](const Torus& t) {
                             auto ind  = rs.transferred_torus_ind.at(handle);
                             auto mat  = obj.value()->transform.local_to_world_matrix();
                             auto r    = math::Vec2f(t.minor_radius, t.major_radius);
                             auto tess = t.tess_level;
                             auto id   = static_cast<int>(handle.obj_id);
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "radii", r.raw_ptr());
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "tessLevel", tess.raw_ptr());
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat", mat[0].raw_ptr());
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat1", mat[1].raw_ptr());
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat2", mat[2].raw_ptr());
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat3", mat[3].raw_ptr());
                             rs.torus_vao.vbo("matrices").set_attribute_value(ind, "id", &id);

                             if (rs.scene_objects_rs.contains(handle)) {
                               auto vs = static_cast<int>(rs.scene_objects_rs.at(handle).state.visibility);
                               rs.torus_vao.vbo("matrices").set_attribute_value(ind, "state", &vs);
                             } else {
                               auto vs = 0;
                               rs.torus_vao.vbo("matrices").set_attribute_value(ind, "state", &vs);
                             }
                           }},
               obj.value()->object);
  }
}

void SceneObjectRSCommandHandler::operator()(const SceneObjectRSCommand::UpdateObjectVisibility& cmd) {
  auto& rs           = renderer.scene_objs_rs_;
  const auto& handle = cmd_ctx.handle;

  if (auto obj = scene.get_obj(handle)) {
    if (!rs.scene_objects_rs.contains(handle)) {
      rs.scene_objects_rs.insert({handle, SceneObjectRS::create(*obj.value())});
    } else {
      rs.scene_objects_rs.at(handle).state.visibility = cmd.new_visibility_state;
    }
  }
}

void SceneObjectRSCommandHandler::operator()(const SceneObjectRSCommand::Internal::DeleteObject&) {
  auto& rs           = renderer.scene_objs_rs_;
  const auto& handle = cmd_ctx.handle;

  if (auto obj = scene.get_obj(handle)) {
    std::visit(
        eray::util::match{
            [&](const Point&) {
              if (rs.transferred_points_buff.size() - 1 == rs.transferred_point_ind.at(handle)) {
                rs.transferred_points_buff.pop_back();
                rs.transferred_point_ind.erase(handle);
                return;
              }

              auto ind = rs.transferred_point_ind.at(handle);
              rs.transferred_point_ind.erase(handle);
              rs.transferred_points_buff[ind] = rs.transferred_points_buff[rs.transferred_points_buff.size() - 1];
              rs.transferred_point_ind[rs.transferred_points_buff[ind]] = ind;
              rs.transferred_points_buff.pop_back();

              auto i = rs.transferred_points_buff[ind].obj_id;
              rs.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
            },
            [&](const Torus&) {
              if (rs.transferred_torus_buff.size() - 1 == rs.transferred_torus_ind.at(handle)) {
                rs.transferred_torus_buff.pop_back();
                rs.transferred_torus_ind.erase(handle);
                return;
              }

              auto ind = rs.transferred_torus_ind[handle];
              rs.transferred_torus_ind.erase(handle);
              rs.transferred_torus_buff[ind] = rs.transferred_torus_buff[rs.transferred_torus_buff.size() - 1];
              rs.transferred_torus_ind[rs.transferred_torus_buff[ind]] = ind;
              rs.transferred_torus_buff.pop_back();
              auto id = static_cast<int>(handle.obj_id);

              if (auto o2 = scene.get_obj(rs.transferred_torus_buff[ind])) {
                auto mat = o2.value()->transform.local_to_world_matrix();
                std::visit(eray::util::match{
                               [&](const Point&) {},
                               [&](const Torus& t) {
                                 auto r    = math::Vec2f(t.minor_radius, t.major_radius);
                                 auto tess = t.tess_level;
                                 rs.torus_vao.vbo("matrices").set_attribute_value(ind, "radii", r.raw_ptr());
                                 rs.torus_vao.vbo("matrices").set_attribute_value(ind, "tessLevel", tess.raw_ptr());
                               },
                           },
                           o2.value()->object);
                rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat", mat[0].raw_ptr());
                rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat1", mat[1].raw_ptr());
                rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat2", mat[2].raw_ptr());
                rs.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat3", mat[3].raw_ptr());
                rs.torus_vao.vbo("matrices").set_attribute_value(ind, "id", &id);
              }
            }},
        obj.value()->object);
  }
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::Internal::AddObject&) {
  auto& rs           = renderer.point_lists_rs_;
  const auto& handle = cmd_ctx.handle;
  if (auto obj = scene.get_obj(handle)) {
    rs.point_lists.insert({handle, PointListObjectRS::create(renderer.scene_objs_rs_.points_vao.vbo(), *obj.value())});
  }
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateObjectMembers&) {
  namespace views  = std::views;
  namespace ranges = std::ranges;

  auto& rs           = renderer.point_lists_rs_;
  const auto& handle = cmd_ctx.handle;

  if (!rs.point_lists.contains(handle)) {
    return;
  }

  if (auto obj = scene.get_obj(handle)) {
    auto point_ids = obj.value()->points() | views::transform([](const SceneObject& ph) { return ph.id(); }) |
                     ranges::to<std::vector>();

    rs.point_lists.at(handle).polyline_ebo.buffer_data(std::span{point_ids}, gl::DataUsage::StaticDraw);

    std::visit(
        util::match{
            [&](MultisegmentBezierCurve&) {
              static auto constexpr kMaxDegree  = MultisegmentBezierCurveRS::kMaxBezierDegree;
              static auto constexpr kWindowSize = MultisegmentBezierCurveRS::kMaxBezierPoints;

              auto thrd_degree_bezier_control_points = point_ids | views::slide(kWindowSize) |
                                                       views::stride(kWindowSize - 1) | views::join |
                                                       ranges::to<std::vector>();

              // Add the last window that has length less than 4
              auto n        = point_ids.size();
              auto last_ind = ((n - 1) / kMaxDegree) * kMaxDegree;
              int degree    = static_cast<int>(n - last_ind - 1);
              for (auto i = last_ind; i < n; ++i) {
                thrd_degree_bezier_control_points.push_back(point_ids[i]);
              }

              // Fill up the last window, so that it has exactly 4 elements
              if (n > 0 && degree > 0) {
                for (int i = 0; i < kMaxDegree - degree; ++i) {
                  thrd_degree_bezier_control_points.push_back(thrd_degree_bezier_control_points.back());
                }
              }

              // Assert that correct rendering state is attached to the handle
              if (!rs.point_lists.at(handle).specialized_rs ||
                  !std::holds_alternative<MultisegmentBezierCurveRS>(*rs.point_lists.at(handle).specialized_rs)) {
                rs.point_lists.at(handle).specialized_rs = MultisegmentBezierCurveRS::create();
              }

              // Update the rendering state
              std::get<MultisegmentBezierCurveRS>(*rs.point_lists.at(handle).specialized_rs).last_bezier_degree =
                  degree;
              std::get<MultisegmentBezierCurveRS>(*rs.point_lists.at(handle).specialized_rs)
                  .control_points_ebo.buffer_data(std::span{thrd_degree_bezier_control_points},
                                                  gl::DataUsage::StaticDraw);
            },
            [&](BSplineCurve&) {
              static auto constexpr kWindowSize = BSplineCurveRS::kMaxBSplinePoints;

              auto de_boor_points =
                  point_ids | views::slide(kWindowSize) | views::stride(1) | views::join | ranges::to<std::vector>();

              // Assert that correct rendering state is attached to the handle
              if (!rs.point_lists.at(handle).specialized_rs ||
                  !std::holds_alternative<BSplineCurveRS>(*rs.point_lists.at(handle).specialized_rs)) {
                rs.point_lists.at(handle).specialized_rs = BSplineCurveRS::create();
              }

              // Update the rendering state
              std::get<BSplineCurveRS>(*rs.point_lists.at(handle).specialized_rs)
                  .de_boor_points_ebo.buffer_data(std::span{de_boor_points}, gl::DataUsage::StaticDraw);
            },
            [](auto&) {},

        },
        obj.value()->object);
  } else {
    rs.point_lists.erase(handle);
  }
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateObjectVisibility&) {
  // TODO(migoox): implement object visibility for point lists
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::Internal::DeleteObject&) {
  auto& rs           = renderer.point_lists_rs_;
  const auto& handle = cmd_ctx.handle;

  rs.point_lists.erase(handle);
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::ShowPolyline& cmd) {
  auto& rs           = renderer.point_lists_rs_;
  const auto& handle = cmd_ctx.handle;

  rs.point_lists.at(handle).state.show_polyline = cmd.show;
}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::ShowBernsteinControlPoints&) {}

void PointListObjectRSCommandHandler::operator()(const PointListObjectRSCommand::UpdateBernsteinControlPoints&) {}

namespace {

gl::VertexArrays create_torus_vao(float width = 1.F, float height = 1.F) {
  float vertices[] = {
      0.5F * width,  0.5F * height,   //
      -0.5F * width, 0.5F * height,   //
      0.5F * width,  -0.5F * height,  //
      -0.5F * width, -0.5F * height,  //
  };

  unsigned int indices[] = {
      0, 1, 2, 3  //
  };

  auto vbo_layout = gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("patchPos", 0, 2);
  auto vbo = gl::VertexBuffer::create(std::move(vbo_layout));
  vbo.buffer_data<float>(vertices, gl::DataUsage::StaticDraw);

  auto mat_vbo_layout = gl::VertexBuffer::Layout();
  mat_vbo_layout.add_attribute<float>("radii", 1, 2);
  mat_vbo_layout.add_attribute<int>("tessLevel", 2, 2);
  mat_vbo_layout.add_attribute<float>("worldMat", 3, 4);
  mat_vbo_layout.add_attribute<float>("worldMat1", 4, 4);
  mat_vbo_layout.add_attribute<float>("worldMat2", 5, 4);
  mat_vbo_layout.add_attribute<float>("worldMat3", 6, 4);
  mat_vbo_layout.add_attribute<int>("id", 7, 1);
  mat_vbo_layout.add_attribute<int>("state", 8, 1);
  auto mat_vbo = gl::VertexBuffer::create(std::move(mat_vbo_layout));

  auto data = std::vector<float>(21 * Scene::kMaxObjects, 0.F);
  mat_vbo.buffer_data(std::span<float>{data}, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  auto m = std::unordered_map<zstring_view, gl::VertexBuffer>();
  m.emplace("base", std::move(vbo));
  m.emplace("matrices", std::move(mat_vbo));
  auto vao = gl::VertexArrays::create(std::move(m), std::move(ebo));

  vao.set_binding_divisor("base", 0);
  vao.set_binding_divisor("matrices", 1);

  return vao;
}

gl::VertexArray create_points_vao() {
  auto points  = std::array<float, 3 * Scene::kMaxObjects>();
  auto indices = std::array<uint32_t, Scene::kMaxObjects>();

  auto vbo_layout = gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  vbo_layout.add_attribute<int>("state", 1, 1);
  auto vbo = gl::VertexBuffer::create(std::move(vbo_layout));

  vbo.buffer_data<float>(points, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  auto vao = gl::VertexArray::create(std::move(vbo), std::move(ebo));
  return vao;
}

gl::TextureHandle create_texture(const res::Image& image) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
               image.raw());
  glBindTexture(GL_TEXTURE_2D, 0);

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
  TRY_UNWRAP_ASSET(bezier_tesc, manager.load_shader(shaders_path / "curves" / "bezier_c0.tesc"));
  TRY_UNWRAP_ASSET(bezier_tese, manager.load_shader(shaders_path / "curves" / "bezier_c0.tese"));
  TRY_UNWRAP_ASSET(bezier_frag, manager.load_shader(shaders_path / "utils" / "solid_color.frag"));
  TRY_UNWRAP_PROGRAM(bezier_prog,
                     gl::RenderingShaderProgram::create("bezier_shader", std::move(bezier_vert), std::move(bezier_frag),
                                                        std::move(bezier_tesc), std::move(bezier_tese)));

  TRY_UNWRAP_ASSET(bspline_vert, manager.load_shader(shaders_path / "curves" / "curve.vert"));
  TRY_UNWRAP_ASSET(bspline_tesc, manager.load_shader(shaders_path / "curves" / "bspline_c2.tesc"));
  TRY_UNWRAP_ASSET(bspline_tese, manager.load_shader(shaders_path / "curves" / "bspline_c2.tese"));
  TRY_UNWRAP_ASSET(bspline_frag, manager.load_shader(shaders_path / "utils" / "solid_color.frag"));
  TRY_UNWRAP_PROGRAM(bspline_prog, gl::RenderingShaderProgram::create("bspline_shader", std::move(bspline_vert),
                                                                      std::move(bspline_frag), std::move(bspline_tesc),
                                                                      std::move(bspline_tese)));

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

  TRY_UNWRAP_ASSET(point_img, eray::res::Image::load_from_path(assets_path / "img" / "point.png"));

  TRY_UNWRAP_ASSET(screen_quad_vert, manager.load_shader(shaders_path / "utils" / "screen_quad.vert"));
  TRY_UNWRAP_ASSET(screen_quad_frag, manager.load_shader(shaders_path / "utils" / "screen_quad.frag"));
  TRY_UNWRAP_PROGRAM(screen_quad_prog,
                     gl::RenderingShaderProgram::create("screen_quad_shader", std::move(screen_quad_vert),
                                                        std::move(screen_quad_frag)));

  auto shaders = Shaders{
      .param            = std::move(param_prog),             //
      .grid             = std::move(grid_prog),              //
      .polyline         = std::move(polyline_prog),          //
      .bezier           = std::move(bezier_prog),            //
      .bspline          = std::move(bspline_prog),           //
      .sprite           = std::move(sprite_prog),            //
      .instanced_sprite = std::move(instanced_sprite_prog),  //
      .screen_quad      = std::move(screen_quad_prog),       //
  };

  auto global_rs = GlobalRS{
      .billboards = {},    //
      .show_grid  = true,  //
  };

  auto scene_objs_rs = SceneObjectsRS{
      .points_vao = create_points_vao(),        //
      .torus_vao  = create_torus_vao(),         //
      .point_txt  = create_texture(point_img),  //
      .cmds       = {},                         //
  };

  auto point_list_objs = PointListObjectsRS{
      .point_lists = {},
      .cmds        = {},
  };

  return std::unique_ptr<ISceneRenderer>(new OpenGLSceneRenderer(
      std::move(shaders), std::move(global_rs), std::move(scene_objs_rs), std::move(point_list_objs),
      std::make_unique<gl::ViewportFramebuffer>(win_size.x, win_size.y)));
}
OpenGLSceneRenderer::OpenGLSceneRenderer(Shaders&& shaders, GlobalRS&& global_rs, SceneObjectsRS&& objs_rs,
                                         PointListObjectsRS&& point_list_objs_rs,
                                         std::unique_ptr<eray::driver::gl::ViewportFramebuffer>&& framebuffer)
    : shaders_(std::move(shaders)),
      global_rs_(std::move(global_rs)),
      scene_objs_rs_(std::move(objs_rs)),
      point_lists_rs_(std::move(point_list_objs_rs)),
      framebuffer_(std::move(framebuffer)) {}

void OpenGLSceneRenderer::push_object_rs_cmd(const SceneObjectRSCommand& cmd) { scene_objs_rs_.cmds.push_back(cmd); }

std::optional<::mini::SceneObjectRS> OpenGLSceneRenderer::object_rs(const SceneObjectHandle& handle) {
  if (!scene_objs_rs_.scene_objects_rs.contains(handle)) {
    return std::nullopt;
  }

  return scene_objs_rs_.scene_objects_rs.at(handle).state;
}

void OpenGLSceneRenderer::set_object_rs(const SceneObjectHandle& handle, const ::mini::SceneObjectRS& state) {
  if (scene_objs_rs_.scene_objects_rs.contains(handle)) {
    scene_objs_rs_.scene_objects_rs.at(handle).state = state;
  }
}

std::optional<::mini::PointListObjectRS> OpenGLSceneRenderer::object_rs(const PointListObjectHandle& handle) {
  if (!point_lists_rs_.point_lists.contains(handle)) {
    return std::nullopt;
  }

  return point_lists_rs_.point_lists.at(handle).state;
}

void OpenGLSceneRenderer::set_object_rs(const PointListObjectHandle& handle, const ::mini::PointListObjectRS& state) {
  if (point_lists_rs_.point_lists.contains(handle)) {
    point_lists_rs_.point_lists.at(handle).state = state;
  }
}

void OpenGLSceneRenderer::push_object_rs_cmd(const PointListObjectRSCommand& cmd) {
  point_lists_rs_.cmds.push_back(cmd);
}

void OpenGLSceneRenderer::add_billboard(zstring_view name, const eray::res::Image& img) {
  auto txt = create_texture(img);
  global_rs_.billboards.insert({name, BillboardRS(std::move(txt))});
}

::mini::BillboardRS& OpenGLSceneRenderer::billboard(zstring_view name) { return global_rs_.billboards.at(name).state; }

void OpenGLSceneRenderer::resize_viewport(eray::math::Vec2i win_size) { framebuffer_->resize(win_size.x, win_size.y); }

void OpenGLSceneRenderer::update(Scene& scene) {
  for (const auto& cmd_ctx : scene_objs_rs_.cmds) {
    std::visit(SceneObjectRSCommandHandler(*this, scene, cmd_ctx), cmd_ctx.cmd);
  }
  scene_objs_rs_.cmds.clear();

  for (const auto& cmd_ctx : point_lists_rs_.cmds) {
    std::visit(PointListObjectRSCommandHandler(*this, scene, cmd_ctx), cmd_ctx.cmd);
  }
  point_lists_rs_.cmds.clear();
}

std::unordered_set<int> OpenGLSceneRenderer::sample_mouse_pick_box(size_t x, size_t y, size_t width,
                                                                   size_t height) const {
  framebuffer_->bind();
  return framebuffer_->sample_mouse_pick_box(x, y, width, height);
}

void OpenGLSceneRenderer::render(Camera& camera) {
  framebuffer_->bind();
  framebuffer_->clear_pick_render();

  // Prepare the framebuffer
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.09, 0.05, 0.09, 1.F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render toruses
  shaders_.param->set_uniform("u_vMat", camera.view_matrix());
  shaders_.param->set_uniform("u_pMat", camera.proj_matrix());
  shaders_.param->bind();
  scene_objs_rs_.torus_vao.bind();

  framebuffer_->begin_pick_render();
  glDepthMask(GL_FALSE);
  shaders_.param->set_uniform("u_fill", true);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(scene_objs_rs_.torus_vao.ebo().count()), GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(scene_objs_rs_.transferred_torus_buff.size()));
  glDepthMask(GL_TRUE);
  framebuffer_->end_pick_render();

  shaders_.param->set_uniform("u_fill", false);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(scene_objs_rs_.torus_vao.ebo().count()), GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(scene_objs_rs_.transferred_torus_buff.size()));

  // Render polylines
  shaders_.polyline->bind();
  shaders_.polyline->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  for (auto& point_list : point_lists_rs_.point_lists) {
    if (point_list.second.state.show_polyline) {
      glVertexArrayElementBuffer(point_list.second.vao.get(), point_list.second.polyline_ebo.raw_gl_id());
      glBindVertexArray(point_list.second.vao.get());
      glDrawElements(GL_LINE_STRIP, static_cast<GLsizei>(point_list.second.polyline_ebo.count()), GL_UNSIGNED_INT,
                     nullptr);
    }
  }

  // Render Beziers
  shaders_.bezier->bind();
  shaders_.bezier->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  shaders_.bezier->set_uniform("u_width", static_cast<float>(framebuffer_->width()));
  shaders_.bezier->set_uniform("u_height", static_cast<float>(framebuffer_->height()));
  shaders_.bezier->set_uniform("u_color", math::Vec4f(1.F, 0.59F, 0.4F, 1.F));
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  for (auto& point_list : point_lists_rs_.point_lists) {
    if (!point_list.second.specialized_rs) {
      continue;
    }

    std::visit(util::match{
                   [&](const MultisegmentBezierCurveRS& s) {
                     glVertexArrayElementBuffer(point_list.second.vao.get(), s.control_points_ebo.raw_gl_id());
                     glBindVertexArray(point_list.second.vao.get());

                     shaders_.bezier->set_uniform("u_bezier_degree", 3);
                     if (s.last_bezier_degree > 0) {
                       glDrawElements(GL_PATCHES,
                                      s.control_points_ebo.count() - MultisegmentBezierCurveRS::kMaxBezierPoints,
                                      GL_UNSIGNED_INT, nullptr);

                       shaders_.bezier->set_uniform("u_bezier_degree", s.last_bezier_degree);
                       glDrawElements(GL_PATCHES, MultisegmentBezierCurveRS::kMaxBezierPoints, GL_UNSIGNED_INT,
                                      (const void*)(sizeof(GLuint) * (s.control_points_ebo.count() -
                                                                      MultisegmentBezierCurveRS::kMaxBezierPoints)));
                     } else {
                       glDrawElements(GL_PATCHES, s.control_points_ebo.count(), GL_UNSIGNED_INT, nullptr);
                     }
                   },
                   [](const auto&) {},
               },
               *point_list.second.specialized_rs);
  }

  // Render B-Splines
  shaders_.bspline->bind();
  shaders_.bspline->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  shaders_.bspline->set_uniform("u_width", static_cast<float>(framebuffer_->width()));
  shaders_.bspline->set_uniform("u_height", static_cast<float>(framebuffer_->height()));
  shaders_.bspline->set_uniform("u_color", math::Vec4f(1.F, 0.59F, 0.4F, 1.F));
  for (auto& point_list : point_lists_rs_.point_lists) {
    if (!point_list.second.specialized_rs) {
      continue;
    }

    std::visit(util::match{
                   [&](const BSplineCurveRS& s) {
                     glVertexArrayElementBuffer(point_list.second.vao.get(), s.de_boor_points_ebo.raw_gl_id());
                     glBindVertexArray(point_list.second.vao.get());
                     glDrawElements(GL_PATCHES, s.de_boor_points_ebo.count(), GL_UNSIGNED_INT, nullptr);
                   },
                   [](const auto&) {},
               },
               *point_list.second.specialized_rs);
  }

  // Render grid
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  if (global_rs_.show_grid) {
    shaders_.grid->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
    shaders_.grid->set_uniform("u_vInvMat", camera.inverse_view_matrix());
    shaders_.grid->set_uniform("u_camWorldPos", camera.transform.pos());
    shaders_.grid->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  // Render points
  framebuffer_->begin_pick_render();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, scene_objs_rs_.point_txt.get());
  shaders_.instanced_sprite->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  shaders_.instanced_sprite->set_uniform("u_scale", 0.03F);
  shaders_.instanced_sprite->set_uniform("u_aspectRatio", camera.aspect_ratio());
  shaders_.instanced_sprite->set_uniform("u_textureSampler", 0);
  shaders_.instanced_sprite->bind();
  scene_objs_rs_.points_vao.bind();
  glDrawElements(GL_POINTS, scene_objs_rs_.transferred_points_buff.size(), GL_UNSIGNED_INT, nullptr);
  framebuffer_->end_pick_render();

  // Render billboards
  glDisable(GL_DEPTH_TEST);
  for (auto& [name, billboard] : global_rs_.billboards) {
    if (!billboard.state.show) {
      continue;
    }

    glBindTexture(GL_TEXTURE_2D, billboard.texture.get());
    shaders_.sprite->set_uniform("u_worldPos", billboard.state.position);
    shaders_.sprite->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
    shaders_.sprite->set_uniform("u_aspectRatio", camera.aspect_ratio());
    shaders_.sprite->set_uniform("u_scale", billboard.state.scale);
    shaders_.sprite->set_uniform("u_textureSampler", 0);
    shaders_.sprite->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  // Render to the default framebuffer
  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, framebuffer_->color_texture());
  glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  shaders_.screen_quad->bind();
  shaders_.screen_quad->set_uniform("u_textureSampler", 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

}  // namespace mini::gl
