#include <liberay/util/logger.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/param_primitive_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/param_primitive.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini::gl {

namespace util = eray::util;
namespace math = eray::math;

void ParamPrimitiveRSCommandHandler::operator()(const ParamPrimitiveRSCommand::Internal::AddObject&) {
  if (auto o = scene.arena<ParamPrimitive>().get_obj(cmd_ctx.handle)) {
    renderer.rs_.emplace(cmd_ctx.handle, ParamPrimitiveRS());
    auto ind = renderer.m_.transferred_torus_buff.size();
    renderer.m_.transferred_torus_ind.insert({cmd_ctx.handle, ind});
    renderer.m_.transferred_torus_buff.push_back(cmd_ctx.handle);
  }
}

void ParamPrimitiveRSCommandHandler::operator()(const ParamPrimitiveRSCommand::Internal::UpdateTrimmingTextures&) {
  // TODO(migoox): trimming support
}

void ParamPrimitiveRSCommandHandler::operator()(const ParamPrimitiveRSCommand::UpdateObjectMembers&) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn(
        "OpenGL Renderer received update UpdateObjectMembers command but there is no rendering state created");
    return;
  }

  auto ind = static_cast<GLuint>(renderer.m_.transferred_torus_ind.at(handle));

  if (auto opt = scene.arena<ParamPrimitive>().get_obj(handle)) {
    auto& obj = **opt;

    auto torus_visitor = [&](const Torus& t) {
      auto mat  = obj.transform().local_to_world_matrix();
      auto r    = math::Vec2f(t.minor_radius, t.major_radius);
      auto tess = t.tess_level;
      auto id   = static_cast<int>(handle.obj_id);
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "radii", r.raw_ptr());
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "tessLevel", tess.raw_ptr());
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat", mat[0].raw_ptr());
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat1", mat[1].raw_ptr());
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat2", mat[2].raw_ptr());
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat3", mat[3].raw_ptr());
      renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "id", &id);
    };

    std::visit(util::match{torus_visitor}, obj.object);
  }
}

void ParamPrimitiveRSCommandHandler::operator()(const ParamPrimitiveRSCommand::UpdateObjectVisibility& cmd) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn(
        "OpenGL Renderer received update UpdateObjectVisibility command but there is no rendering state created");
    return;
  }

  auto& obj_rs = renderer.rs_.at(handle);

  obj_rs.visibility = cmd.new_visibility_state;
  auto vs           = static_cast<int>(obj_rs.visibility);

  auto ind = static_cast<GLuint>(renderer.m_.transferred_torus_ind.at(handle));
  renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "state", &vs);
}

void ParamPrimitiveRSCommandHandler::operator()(const ParamPrimitiveRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn("OpenGL Renderer received update DeleteObject command but there is no rendering state created");
    return;
  }

  if (renderer.m_.transferred_torus_buff.size() - 1 == renderer.m_.transferred_torus_ind.at(handle)) {
    renderer.m_.transferred_torus_buff.pop_back();
    renderer.m_.transferred_torus_ind.erase(handle);
    return;
  }

  auto ind = static_cast<GLuint>(renderer.m_.transferred_torus_ind[handle]);
  renderer.m_.transferred_torus_ind.erase(handle);
  renderer.m_.transferred_torus_buff[ind] =
      renderer.m_.transferred_torus_buff[renderer.m_.transferred_torus_buff.size() - 1];
  renderer.m_.transferred_torus_ind[renderer.m_.transferred_torus_buff[ind]] = ind;
  renderer.m_.transferred_torus_buff.pop_back();
  auto id = static_cast<int>(handle.obj_id);

  if (auto o2 = scene.arena<ParamPrimitive>().get_obj(renderer.m_.transferred_torus_buff[ind])) {
    auto mat = o2.value()->transform().local_to_world_matrix();
    std::visit(
        eray::util::match{
            [&](const Torus& t) {
              auto r    = math::Vec2f(t.minor_radius, t.major_radius);
              auto tess = t.tess_level;
              renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "radii", r.raw_ptr());
              renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "tessLevel", tess.raw_ptr());
            },
        },
        o2.value()->object);
    renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat", mat[0].raw_ptr());
    renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat1", mat[1].raw_ptr());
    renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat2", mat[2].raw_ptr());
    renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "worldMat3", mat[3].raw_ptr());
    renderer.m_.torus_vao.vbo("matrices").set_attribute_value(ind, "id", &id);
  }

  renderer.rs_.erase(handle);
}

}  // namespace mini::gl

namespace {

eray::driver::gl::VertexArrays create_torus_vao(float width = 1.F, float height = 1.F) {
  float vertices[] = {
      0.5F * width,  0.5F * height,   //
      -0.5F * width, 0.5F * height,   //
      0.5F * width,  -0.5F * height,  //
      -0.5F * width, -0.5F * height,  //
  };

  unsigned int indices[] = {
      0, 1, 2, 3  //
  };

  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("patchPos", 0, 2);
  auto vbo = eray::driver::gl::VertexBuffer::create(std::move(vbo_layout));
  vbo.buffer_data<float>(vertices, eray::driver::gl::DataUsage::StaticDraw);

  auto mat_vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  mat_vbo_layout.add_attribute<float>("radii", 1, 2);
  mat_vbo_layout.add_attribute<int>("tessLevel", 2, 2);
  mat_vbo_layout.add_attribute<float>("worldMat", 3, 4);
  mat_vbo_layout.add_attribute<float>("worldMat1", 4, 4);
  mat_vbo_layout.add_attribute<float>("worldMat2", 5, 4);
  mat_vbo_layout.add_attribute<float>("worldMat3", 6, 4);
  mat_vbo_layout.add_attribute<int>("id", 7, 1);
  mat_vbo_layout.add_attribute<int>("state", 8, 1);
  auto mat_vbo = eray::driver::gl::VertexBuffer::create(std::move(mat_vbo_layout));

  auto data = std::vector<float>(21 * mini::Scene::kMaxObjects, 0.F);
  mat_vbo.buffer_data(std::span<float>{data}, eray::driver::gl::DataUsage::StaticDraw);

  auto ebo = eray::driver::gl::ElementBuffer::create();
  ebo.buffer_data(indices, eray::driver::gl::DataUsage::StaticDraw);

  auto m = std::unordered_map<zstring_view, eray::driver::gl::VertexBuffer>();
  m.emplace("base", std::move(vbo));
  m.emplace("matrices", std::move(mat_vbo));
  auto vao = eray::driver::gl::VertexArrays::create(std::move(m), std::move(ebo));

  vao.set_binding_divisor("base", 0);
  vao.set_binding_divisor("matrices", 1);

  return vao;
}

}  // namespace

namespace mini::gl {

ParamPrimitiveRenderer ParamPrimitiveRenderer::create() {
  return ParamPrimitiveRenderer(Members{
      .torus_vao              = create_torus_vao(),  //
      .transferred_torus_buff = {},
      .transferred_torus_ind  = {},
  });
}

ParamPrimitiveRenderer::ParamPrimitiveRenderer(Members&& members) : m_(std::move(members)) {}

void ParamPrimitiveRenderer::update_impl(Scene& /*scene*/) {}

void ParamPrimitiveRenderer::render_parameterized_surfaces() const {
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
  m_.torus_vao.bind();
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(m_.torus_vao.ebo().count()), GL_UNSIGNED_INT, nullptr,
                          static_cast<GLsizei>(m_.transferred_torus_buff.size()));
}

void ParamPrimitiveRenderer::render_parameterized_surfaces_filled() const {
  ERAY_GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  ERAY_GL_CALL(glDepthMask(GL_FALSE));
  m_.torus_vao.bind();
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(m_.torus_vao.ebo().count()), GL_UNSIGNED_INT, nullptr,
                          static_cast<GLsizei>(m_.transferred_torus_buff.size()));
  ERAY_GL_CALL(glDepthMask(GL_TRUE));
}

}  // namespace mini::gl
