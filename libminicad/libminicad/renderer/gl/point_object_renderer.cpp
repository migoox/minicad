#include <liberay/util/logger.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/renderer/gl/point_object_renderer.hpp>
#include <libminicad/renderer/gl/rendering_state.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/rendering_state.hpp>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object.hpp>

namespace mini::gl {

namespace util = eray::util;

void PointObjectRSCommandHandler::operator()(const PointObjectRSCommand::Internal::AddObject&) {
  if (auto o = scene.arena<PointObject>().get_obj(cmd_ctx.handle)) {
    renderer.rs_.emplace(cmd_ctx.handle, PointObjectRS());

    auto i   = cmd_ctx.handle.obj_id;
    auto ind = static_cast<GLuint>(renderer.m_.transferred_points_buff.size());
    renderer.m_.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
    renderer.m_.transferred_point_ind.insert({cmd_ctx.handle, ind});
    renderer.m_.transferred_points_buff.push_back(cmd_ctx.handle);
  }
}

void PointObjectRSCommandHandler::operator()(const PointObjectRSCommand::UpdateObjectMembers&) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn(
        "OpenGL Renderer received update UpdateObjectMembers command but there is no rendering state created");
    return;
  }

  if (auto o = scene.arena<PointObject>().get_obj(cmd_ctx.handle)) {
    auto p = o.value()->transform().pos();
    renderer.m_.points_vao.vbo().set_attribute_value(handle.obj_id, "pos", p.raw_ptr());
  }
}

void PointObjectRSCommandHandler::operator()(const PointObjectRSCommand::UpdateObjectVisibility& cmd) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn(
        "OpenGL Renderer received update UpdateObjectVisibility command but there is no rendering state created");
    return;
  }

  auto& obj_rs = renderer.rs_.at(handle);

  obj_rs.visibility = cmd.new_visibility_state;
  auto vs           = static_cast<int>(obj_rs.visibility);
  renderer.m_.points_vao.vbo().set_attribute_value(handle.obj_id, "state", &vs);
}

void PointObjectRSCommandHandler::operator()(const PointObjectRSCommand::Internal::DeleteObject&) {
  const auto& handle = cmd_ctx.handle;
  if (!renderer.rs_.contains(handle)) {
    util::Logger::warn("OpenGL Renderer received update DeleteObject command but there is no rendering state created");
    return;
  }

  if (renderer.m_.transferred_points_buff.size() - 1 == renderer.m_.transferred_point_ind.at(handle)) {
    renderer.m_.transferred_points_buff.pop_back();
    renderer.m_.transferred_point_ind.erase(handle);
    return;
  }

  auto ind = static_cast<GLuint>(renderer.m_.transferred_point_ind.at(handle));
  renderer.m_.transferred_point_ind.erase(handle);
  renderer.m_.transferred_points_buff[ind] =
      renderer.m_.transferred_points_buff[renderer.m_.transferred_points_buff.size() - 1];
  renderer.m_.transferred_point_ind[renderer.m_.transferred_points_buff[ind]] = ind;
  renderer.m_.transferred_points_buff.pop_back();

  auto i = renderer.m_.transferred_points_buff[ind].obj_id;
  renderer.m_.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
  renderer.rs_.erase(handle);
}

namespace {

eray::driver::gl::VertexArray create_points_vao() {
  auto points  = std::array<float, 3 * Scene::kMaxObjects>();
  auto indices = std::array<uint32_t, Scene::kMaxObjects>();

  auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
  vbo_layout.add_attribute<float>("pos", 0, 3);
  vbo_layout.add_attribute<int>("state", 1, 1);
  auto vbo = eray::driver::gl::VertexBuffer::create(std::move(vbo_layout));

  vbo.buffer_data<float>(points, eray::driver::gl::DataUsage::StaticDraw);

  auto ebo = eray::driver::gl::ElementBuffer::create();
  ebo.buffer_data(indices, eray::driver::gl::DataUsage::StaticDraw);

  auto vao = eray::driver::gl::VertexArray::create(std::move(vbo), std::move(ebo));
  return vao;
}

}  // namespace

PointObjectRenderer PointObjectRenderer::create() {
  return PointObjectRenderer(Members{
      .points_vao              = create_points_vao(),  //
      .transferred_points_buff = {},
      .transferred_point_ind   = {},
  });
}

PointObjectRenderer::PointObjectRenderer(Members&& members) : m_(std::move(members)) {}

void PointObjectRenderer::update_impl(Scene& /*scene*/) {}

void PointObjectRenderer::render_control_points() const {
  m_.points_vao.bind();
  ERAY_GL_CALL(glDrawElements(GL_POINTS, m_.transferred_points_buff.size(), GL_UNSIGNED_INT, nullptr));
}

}  // namespace mini::gl
