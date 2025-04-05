#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene.hpp>
#include <variant>

namespace mini {

namespace driver = eray::driver;
namespace gl     = eray::driver::gl;
namespace util   = eray::util;
namespace res    = eray::res;
namespace math   = eray::math;

namespace {
gl::VertexArray create_box_vao(float width = 1.F, float height = 1.F, float depth = 1.F) {
  float vertices[] = {
      // Front Face
      -0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //
      +0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //
      +0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //
      -0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 0.0F, -1.0F,  //

      // Back Face
      +0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //
      -0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //
      -0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //
      +0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 0.0F, 1.0F,  //

      // Left Face
      -0.5F * width, -0.5F * height, +0.5F * depth, -1.0F, 0.0F, 0.0F,  //
      -0.5F * width, -0.5F * height, -0.5F * depth, -1.0F, 0.0F, 0.0F,  //
      -0.5F * width, +0.5F * height, -0.5F * depth, -1.0F, 0.0F, 0.0F,  //
      -0.5F * width, +0.5F * height, +0.5F * depth, -1.0F, 0.0F, 0.0F,  //

      // Right Face
      +0.5F * width, -0.5F * height, -0.5F * depth, 1.0F, 0.0F, 0.0F,  //
      +0.5F * width, -0.5F * height, +0.5F * depth, 1.0F, 0.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, +0.5F * depth, 1.0F, 0.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, -0.5F * depth, 1.0F, 0.0F, 0.0F,  //

      // Bottom Face
      -0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, -1.0F, 0.0F,  //
      +0.5F * width, -0.5F * height, +0.5F * depth, 0.0F, -1.0F, 0.0F,  //
      +0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, -1.0F, 0.0F,  //
      -0.5F * width, -0.5F * height, -0.5F * depth, 0.0F, -1.0F, 0.0F,  //

      // Top Face
      -0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 1.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, -0.5F * depth, 0.0F, 1.0F, 0.0F,  //
      +0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 1.0F, 0.0F,  //
      -0.5F * width, +0.5F * height, +0.5F * depth, 0.0F, 1.0F, 0.0F   //

  };

  unsigned int indices[] = {
      0,  2,  1,  0,  3,  2,   //
      4,  6,  5,  4,  7,  6,   //
      8,  10, 9,  8,  11, 10,  //
      12, 14, 13, 12, 15, 14,  //
      16, 18, 17, 16, 19, 18,  //
      20, 22, 21, 20, 23, 22   //
  };

  auto vbo_layout = gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  vbo_layout.add_attribute<float>("normal", 0, 3);
  auto vbo = gl::VertexBuffer::create(std::move(vbo_layout));

  vbo.buffer_data<float>(vertices, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  return gl::VertexArray::create(std::move(vbo), std::move(ebo));
}

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

std::pair<gl::VertexArrayHandle, gl::ElementBuffer> create_point_list_vao(const gl::VertexBuffer& vert_buff) {
  GLuint id = 0;
  glCreateVertexArrays(1, &id);

  auto ebo = gl::ElementBuffer::create();

  // Bind EBO to VAO
  glVertexArrayElementBuffer(id, ebo.raw_gl_id());

  // Apply layouts of VBO
  GLsizei vertex_size = 0;

  for (const auto& attrib : vert_buff.layout()) {
    glEnableVertexArrayAttrib(id, attrib.location);
    glVertexArrayAttribFormat(id,                                     //
                              attrib.location,                        //
                              static_cast<GLint>(attrib.count),       //
                              GL_FLOAT,                               //
                              attrib.normalize ? GL_TRUE : GL_FALSE,  //
                              vertex_size);                           //

    vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
    glVertexArrayAttribBinding(id, attrib.location, 0);
  }

  glVertexArrayVertexBuffer(id, 0, vert_buff.raw_gl_id(), 0, vertex_size);

  eray::driver::gl::check_gl_errors();

  return {gl::VertexArrayHandle(id), std::move(ebo)};
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

std::expected<SceneRenderer, SceneRenderer::SceneRendererCreationError> SceneRenderer::create(
    const std::filesystem::path& assets_path) {
  auto manager = driver::GLSLShaderManager();

  // NOLINTBEGIN
#define TRY_UNWRAP_ASSET(var, expr) \
  TRY_UNWRAP_DEFINE_TRANSFORM_ERR(var, expr, SceneRendererCreationError::AssetsLoadingFailed)

#define TRY_UNWRAP_PROGRAM(var, expr) \
  TRY_UNWRAP_DEFINE_TRANSFORM_ERR(var, expr, SceneRendererCreationError::ShaderProgramCreationFailed)
  // NOLINTEND

  TRY_UNWRAP_ASSET(param_vert, manager.load_shader(assets_path / "param.vert"));
  TRY_UNWRAP_ASSET(param_frag, manager.load_shader(assets_path / "param.frag"));
  TRY_UNWRAP_ASSET(param_tesc, manager.load_shader(assets_path / "param.tesc"));
  TRY_UNWRAP_ASSET(param_tese, manager.load_shader(assets_path / "param.tese"));
  TRY_UNWRAP_PROGRAM(param_prog,
                     gl::RenderingShaderProgram::create("param_shader", std::move(param_vert), std::move(param_frag),
                                                        std::move(param_tesc), std::move(param_tese)));

  TRY_UNWRAP_ASSET(grid_vert, manager.load_shader(assets_path / "grid.vert"));
  TRY_UNWRAP_ASSET(grid_frag, manager.load_shader(assets_path / "grid.frag"));
  TRY_UNWRAP_PROGRAM(grid_prog,
                     gl::RenderingShaderProgram::create("grid_shader", std::move(grid_vert), std::move(grid_frag)));

  TRY_UNWRAP_ASSET(polyline_vert, manager.load_shader(assets_path / "polyline.vert"));
  TRY_UNWRAP_ASSET(polyline_frag, manager.load_shader(assets_path / "polyline.frag"));
  TRY_UNWRAP_PROGRAM(polyline_prog, gl::RenderingShaderProgram::create("polyline_shader", std::move(polyline_vert),
                                                                       std::move(polyline_frag)));
  TRY_UNWRAP_ASSET(sprite_vert, manager.load_shader(assets_path / "sprite.vert"));
  TRY_UNWRAP_ASSET(sprite_frag, manager.load_shader(assets_path / "sprite.frag"));
  TRY_UNWRAP_PROGRAM(
      sprite_prog, gl::RenderingShaderProgram::create("sprite_shader", std::move(sprite_vert), std::move(sprite_frag)));

  TRY_UNWRAP_ASSET(instanced_sprite_vert, manager.load_shader(assets_path / "sprite_instanced.vert"));
  TRY_UNWRAP_ASSET(instanced_sprite_frag, manager.load_shader(assets_path / "sprite_instanced.frag"));
  TRY_UNWRAP_ASSET(instanced_sprite_geom, manager.load_shader(assets_path / "sprite_instanced.geom"));
  TRY_UNWRAP_PROGRAM(instanced_sprite_prog,
                     gl::RenderingShaderProgram::create("instanced_sprite_shader", std::move(instanced_sprite_vert),
                                                        std::move(instanced_sprite_frag), std::nullopt, std::nullopt,
                                                        std::move(instanced_sprite_geom)));

  TRY_UNWRAP_ASSET(point_img, eray::res::Image::load_from_path(assets_path / "point.png"));
  return SceneRenderer({
      .points_vao               = create_points_vao(),               //
      .box_vao                  = create_box_vao(),                  //
      .torus_vao                = create_torus_vao(),                //
      .point_txt                = create_texture(point_img),         //
      .param_sh_prog            = std::move(param_prog),             //
      .grid_sh_prog             = std::move(grid_prog),              //
      .polyline_sh_prog         = std::move(polyline_prog),          //
      .sprite_sh_prog           = std::move(sprite_prog),            //
      .instanced_sprite_sh_prog = std::move(instanced_sprite_prog),  //
  });
}
SceneRenderer::SceneRenderer(SceneRenderer::RenderingState&& rs) : rs_(std::move(rs)) {}

void SceneRenderer::update_scene_object(const SceneObject& obj) {
  std::visit(util::match{[&](const Point&) {
                           auto p = obj.transform.pos();
                           rs_.points_vao.vbo().set_attribute_value(obj.id(), "pos", p.raw_ptr());

                           if (rs_.visibility_states.contains(obj.handle())) {
                             auto vs = static_cast<int>(rs_.visibility_states.at(obj.handle()));
                             rs_.points_vao.vbo().set_attribute_value(obj.id(), "state", &vs);
                           } else {
                             auto vs = 0;
                             rs_.points_vao.vbo().set_attribute_value(obj.id(), "state", &vs);
                           }
                         },
                         [&](const Torus& t) {
                           auto ind  = rs_.transferred_torus_ind.at(obj.handle());
                           auto mat  = obj.transform.local_to_world_matrix();
                           auto r    = math::Vec2f(t.minor_radius, t.major_radius);
                           auto tess = t.tess_level;
                           auto id   = static_cast<int>(obj.handle().obj_id);
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "radii", r.raw_ptr());
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "tessLevel", tess.raw_ptr());
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat", mat[0].raw_ptr());
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat1", mat[1].raw_ptr());
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat2", mat[2].raw_ptr());
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat3", mat[3].raw_ptr());
                           rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "id", &id);

                           if (rs_.visibility_states.contains(obj.handle())) {
                             auto vs = static_cast<int>(rs_.visibility_states.at(obj.handle()));
                             rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "state", &vs);
                           } else {
                             auto vs = 0;
                             rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "state", &vs);
                           }
                         }},
             obj.object);
}

void SceneRenderer::update_visibility_state(const SceneObjectHandle& handle, VisibilityState state) {
  if (!rs_.visibility_states.contains(handle)) {
    rs_.visibility_states.insert({handle, state});
  } else {
    rs_.visibility_states.at(handle) = state;
  }
}

void SceneRenderer::add_scene_object(const SceneObject& obj) {
  std::visit(eray::util::match{
                 [&](const Point&) {
                   auto i   = obj.id();
                   auto ind = rs_.transferred_points_buff.size();
                   rs_.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
                   rs_.transferred_point_ind.insert({obj.handle(), ind});
                   rs_.transferred_points_buff.push_back(obj.handle());
                 },  //
                 [&](const Torus&) {
                   auto ind = rs_.transferred_torus_buff.size();
                   rs_.transferred_torus_ind.insert({obj.handle(), ind});
                   rs_.transferred_torus_buff.push_back(obj.handle());
                 }  //
             },
             obj.object);
}

void SceneRenderer::add_point_list_object(const PointListObject& obj) {
  rs_.point_list_vaos.insert({obj.handle(), create_point_list_vao(rs_.points_vao.vbo())});
}

void SceneRenderer::delete_scene_object(const SceneObject& obj, Scene& scene) {
  std::visit(
      eray::util::match{
          [&](const Point&) {
            if (rs_.transferred_points_buff.size() - 1 == rs_.transferred_point_ind.at(obj.handle())) {
              rs_.transferred_points_buff.pop_back();
              rs_.transferred_point_ind.erase(obj.handle());
              return;
            }

            auto ind = rs_.transferred_point_ind.at(obj.handle());
            rs_.transferred_point_ind.erase(obj.handle());
            rs_.transferred_points_buff[ind] = rs_.transferred_points_buff[rs_.transferred_points_buff.size() - 1];
            rs_.transferred_point_ind[rs_.transferred_points_buff[ind]] = ind;
            rs_.transferred_points_buff.pop_back();

            auto i = rs_.transferred_points_buff[ind].obj_id;
            rs_.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
          },
          [&](const Torus&) {
            if (rs_.transferred_torus_buff.size() - 1 == rs_.transferred_torus_ind.at(obj.handle())) {
              rs_.transferred_torus_buff.pop_back();
              rs_.transferred_torus_ind.erase(obj.handle());
              return;
            }

            auto ind = rs_.transferred_torus_ind[obj.handle()];
            rs_.transferred_torus_ind.erase(obj.handle());
            rs_.transferred_torus_buff[ind] = rs_.transferred_torus_buff[rs_.transferred_torus_buff.size() - 1];
            rs_.transferred_torus_ind[rs_.transferred_torus_buff[ind]] = ind;
            rs_.transferred_torus_buff.pop_back();
            auto id = static_cast<int>(obj.handle().obj_id);

            if (auto o2 = scene.get_obj(rs_.transferred_torus_buff[ind])) {
              auto mat = o2.value()->transform.local_to_world_matrix();
              std::visit(eray::util::match{
                             [&](const Point&) {},
                             [&](const Torus& t) {
                               auto r    = math::Vec2f(t.minor_radius, t.major_radius);
                               auto tess = t.tess_level;
                               rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "radii", r.raw_ptr());
                               rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "tessLevel", tess.raw_ptr());
                             },
                         },
                         o2.value()->object);
              rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat", mat[0].raw_ptr());
              rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat1", mat[1].raw_ptr());
              rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat2", mat[2].raw_ptr());
              rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat3", mat[3].raw_ptr());
              rs_.torus_vao.vbo("matrices").set_attribute_value(ind, "id", &id);
            }
          }},
      obj.object);
}

void SceneRenderer::delete_point_list_object(const PointListObject& obj) { rs_.point_list_vaos.erase(obj.handle()); }

void SceneRenderer::add_billboard(zstring_view name, const eray::res::Image& img) {
  auto txt = create_texture(img);
  rs_.billboards.insert({name, Billboard(std::move(txt))});
}

Billboard& SceneRenderer::billboard(zstring_view name) { return rs_.billboards.at(name); }

void SceneRenderer::update_point_list_object(const PointListObject& obj) {
  // TODO(migoox): do it better
  if (!rs_.point_list_vaos.contains(obj.handle())) {
    return;
  }
  auto t = obj.points() | std::views::transform([](const PointHandle& ph) { return ph.obj_id; });
  std::vector<PointListObjectId> temp_vec(t.begin(), t.end());
  rs_.point_list_vaos.at(obj.handle()).second.buffer_data(std::span{temp_vec}, gl::DataUsage::StaticDraw);
}

void SceneRenderer::render(eray::driver::gl::ViewportFramebuffer& fb, Camera& camera) {
  fb.bind();
  fb.clear_pick_render();

  // Prepare the framebuffer
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.09, 0.05, 0.09, 1.F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render toruses
  rs_.param_sh_prog->set_uniform("u_vMat", camera.view_matrix());
  rs_.param_sh_prog->set_uniform("u_pMat", camera.proj_matrix());
  rs_.param_sh_prog->bind();
  rs_.torus_vao.bind();

  fb.begin_pick_render();
  glDepthMask(GL_FALSE);
  rs_.param_sh_prog->set_uniform("u_fill", true);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(rs_.torus_vao.ebo().count()), GL_UNSIGNED_INT, nullptr,
                          static_cast<GLsizei>(rs_.transferred_torus_buff.size()));
  glDepthMask(GL_TRUE);
  fb.end_pick_render();

  rs_.param_sh_prog->set_uniform("u_fill", false);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(rs_.torus_vao.ebo().count()), GL_UNSIGNED_INT, nullptr,
                          static_cast<GLsizei>(rs_.transferred_torus_buff.size()));

  // Render point lists
  rs_.polyline_sh_prog->bind();
  rs_.polyline_sh_prog->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  for (auto& point_list : rs_.point_list_vaos) {
    glBindVertexArray(point_list.second.first.get());
    glDrawElements(GL_LINE_STRIP, static_cast<GLsizei>(point_list.second.second.count()), GL_UNSIGNED_INT, nullptr);
  }

  // Render grid
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  if (rs_.show_grid) {
    rs_.grid_sh_prog->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
    rs_.grid_sh_prog->set_uniform("u_vInvMat", camera.inverse_view_matrix());
    rs_.grid_sh_prog->set_uniform("u_camWorldPos", camera.transform.pos());
    rs_.grid_sh_prog->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  // Render points
  fb.begin_pick_render();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, rs_.point_txt.get());
  rs_.instanced_sprite_sh_prog->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
  rs_.instanced_sprite_sh_prog->set_uniform("u_scale", 0.03F);
  rs_.instanced_sprite_sh_prog->set_uniform("u_aspectRatio", camera.aspect_ratio());
  rs_.instanced_sprite_sh_prog->set_uniform("u_textureSampler", 0);
  rs_.instanced_sprite_sh_prog->bind();
  rs_.points_vao.bind();
  glDrawElements(GL_POINTS, rs_.transferred_points_buff.size(), GL_UNSIGNED_INT, nullptr);
  fb.end_pick_render();

  // Render billboards
  glDisable(GL_DEPTH_TEST);
  for (auto& [name, billboard] : rs_.billboards) {
    if (!billboard.show) {
      continue;
    }

    glBindTexture(GL_TEXTURE_2D, billboard.texture.get());
    rs_.sprite_sh_prog->set_uniform("u_worldPos", billboard.position);
    rs_.sprite_sh_prog->set_uniform("u_pvMat", camera.proj_matrix() * camera.view_matrix());
    rs_.sprite_sh_prog->set_uniform("u_aspectRatio", camera.aspect_ratio());
    rs_.sprite_sh_prog->set_uniform("u_scale", billboard.scale);
    rs_.sprite_sh_prog->set_uniform("u_textureSampler", 0);
    rs_.sprite_sh_prog->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace mini
