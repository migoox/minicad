#include <glad/gl.h>
#include <imgui/imgui.h>
#include <imguizmo/ImGuizmo.h>

#include <cstdint>
#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/transform3_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/app.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/timer.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <minicad/app.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/imgui/modals.hpp>
#include <optional>
#include <variant>
#include <vector>

#include "imgui/transform_gizmo.hpp"

namespace mini {

// NOLINTBEGIN
using namespace eray;
using namespace eray::driver;
using namespace eray::os;
using namespace eray::util;
// NOLINTEND

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

  auto vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute::create<float>(0, 3, false),  // positions
      gl::VertexBuffer::Attribute::create<float>(1, 3, false),  // normals
  });
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

  auto vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute::create<float>(0, 2, false),
  });
  vbo.buffer_data<float>(vertices, gl::DataUsage::StaticDraw);

  auto mat_vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute::create<float>(1, 2, false),
      gl::VertexBuffer::Attribute::create<int>(2, 2, false),
      gl::VertexBuffer::Attribute::create<float>(3, 4, false),
      gl::VertexBuffer::Attribute::create<float>(4, 4, false),
      gl::VertexBuffer::Attribute::create<float>(5, 4, false),
      gl::VertexBuffer::Attribute::create<float>(6, 4, false),
  });
  auto data    = std::vector<float>(20 * Scene::kMaxObjects, 0.F);
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

  auto vbo = gl::VertexBuffer::create({
      gl::VertexBuffer::Attribute::create<float>(0, 3, false),
  });
  vbo.buffer_data<float>(points, gl::DataUsage::StaticDraw);

  auto ebo = gl::ElementBuffer::create();
  ebo.buffer_data(indices, gl::DataUsage::StaticDraw);

  auto vao = gl::VertexArray::create(std::move(vbo), std::move(ebo));
  return vao;
}

std::pair<GLuint, gl::ElementBuffer> create_point_list_vao(const gl::VertexBuffer& vert_buff) {
  GLuint id = 0;
  glCreateVertexArrays(1, &id);

  auto ebo = gl::ElementBuffer::create();

  // Bind EBO to VAO
  glVertexArrayElementBuffer(id, ebo.raw_gl_id());

  // Apply layouts of VBO
  GLsizei vertex_size = 0;

  for (const auto& attrib : vert_buff.layout()) {
    glEnableVertexArrayAttrib(id, attrib.index);
    glVertexArrayAttribFormat(id,                                     //
                              attrib.index,                           //
                              static_cast<GLint>(attrib.count),       //
                              GL_FLOAT,                               //
                              attrib.normalize ? GL_TRUE : GL_FALSE,  //
                              vertex_size);                           //

    vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
    glVertexArrayAttribBinding(id, attrib.index, 0);
  }

  glVertexArrayVertexBuffer(id, 0, vert_buff.raw_gl_id(), 0, vertex_size);

  eray::driver::gl::check_gl_errors();

  return {id, std::move(ebo)};
}

GLuint create_texture(res::Image& image) {
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

  return texture;
}

}  // namespace

MiniCadApp MiniCadApp::create(std::unique_ptr<Window> window) {
  auto manager = GLSLShaderManager();

  auto param_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.vert"));
  auto param_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.frag"));
  auto param_tesc = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.tesc"));
  auto param_tese = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "param.tese"));
  auto param_prog = unwrap_or_panic(gl::RenderingShaderProgram::create(
      "param_shader", std::move(param_vert), std::move(param_frag), std::move(param_tesc), std::move(param_tese)));

  auto grid_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "grid.vert"));
  auto grid_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "grid.frag"));
  auto grid_prog =
      unwrap_or_panic(gl::RenderingShaderProgram::create("grid_shader", std::move(grid_vert), std::move(grid_frag)));

  auto polyline_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "polyline.vert"));
  auto polyline_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "polyline.frag"));
  auto polyline_prog = unwrap_or_panic(
      gl::RenderingShaderProgram::create("polyline_shader", std::move(polyline_vert), std::move(polyline_frag)));

  auto sprite_vert = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite.vert"));
  auto sprite_frag = unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite.frag"));
  auto sprite_prog = unwrap_or_panic(
      gl::RenderingShaderProgram::create("sprite_shader", std::move(sprite_vert), std::move(sprite_frag)));

  auto screen_quad_vert =
      unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "screen_quad.vert"));
  auto screen_quad_frag =
      unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "screen_quad.frag"));
  auto screen_quad_prog = unwrap_or_panic(gl::RenderingShaderProgram::create(
      "screen_quad_shader", std::move(screen_quad_vert), std::move(screen_quad_frag)));

  auto instanced_sprite_vert =
      unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite_instanced.vert"));
  auto instanced_sprite_frag =
      unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite_instanced.frag"));
  auto instanced_sprite_geom =
      unwrap_or_panic(manager.load_shader(System::executable_dir() / "assets" / "sprite_instanced.geom"));
  auto instanced_sprite_prog = unwrap_or_panic(gl::RenderingShaderProgram::create(
      "instanced_sprite_shader", std::move(instanced_sprite_vert), std::move(instanced_sprite_frag), std::nullopt,
      std::nullopt, std::move(instanced_sprite_geom)));

  auto cursor_img   = unwrap_or_panic(res::Image::load_from_path(System::executable_dir() / "assets" / "cursor.png"));
  auto point_img    = unwrap_or_panic(res::Image::load_from_path(System::executable_dir() / "assets" / "point.png"));
  auto centroid_img = unwrap_or_panic(res::Image::load_from_path(System::executable_dir() / "assets" / "centroid.png"));
  auto camera       = std::make_unique<Camera>(false, math::radians(90.F),
                                               static_cast<float>(window->size().x) / static_cast<float>(window->size().y),
                                               0.1F, 1000.F);
  camera->transform.set_local_pos(math::Vec3f(0.F, 0.F, 4.F));

  auto gimbal = std::make_unique<eray::math::Transform3f>();
  camera->transform.set_parent(*gimbal);

  auto win_size = math::Vec2f(static_cast<float>(window->size().x), static_cast<float>(window->size().y));
  return MiniCadApp(std::move(window),
                    {

                        .cursor        = std::make_unique<Cursor>(),  //
                        .camera        = std::move(camera),           //
                        .camera_gimbal = std::move(gimbal),           //
                        .grid_on       = true,                        //
                        .scene         = Scene(),                     //
                        .rs =
                            {
                                .points_vao               = create_points_vao(),               //
                                .box_vao                  = create_box_vao(),                  //
                                .torus_vao                = create_torus_vao(),                //
                                .cursor_txt               = create_texture(cursor_img),        //
                                .point_txt                = create_texture(point_img),         //
                                .centroid_txt             = create_texture(centroid_img),      //
                                .param_sh_prog            = std::move(param_prog),             //
                                .grid_sh_prog             = std::move(grid_prog),              //
                                .polyline_sh_prog         = std::move(polyline_prog),          //
                                .sprite_sh_prog           = std::move(sprite_prog),            //
                                .screen_quad_sh_prog      = std::move(screen_quad_prog),       //
                                .instanced_sprite_sh_prog = std::move(instanced_sprite_prog),  //
                                .viewport_fb              = std::make_unique<gl::ViewportFramebuffer>(win_size.x,
                                                                                                      win_size.y),  //

                            }  //
                    });
}

MiniCadApp::MiniCadApp(std::unique_ptr<Window> window, Members&& m) : Application(std::move(window)), m_(std::move(m)) {
  ImGui::mini::gizmo::SetImGuiContext(ImGui::GetCurrentContext());

  window_->set_event_callback<WindowResizedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_resize));
  window_->set_event_callback<MouseButtonPressedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_pressed));
  window_->set_event_callback<MouseButtonReleasedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_released));
  window_->set_event_callback<MouseScrolledEvent>(class_method_as_event_callback(this, &MiniCadApp::on_scrolled));
  window_->set_event_callback<KeyPressedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_key_pressed));
}

void MiniCadApp::render_gui(Duration /* delta */) {
  ImGui::Begin("Torus [TEMP]");
  ImGui::Text("FPS: %d", fps_);

  ImGui::Checkbox("Grid", &m_.grid_on);
  ImGui::Checkbox("Ortho", &m_.use_ortho);

  ImGui::End();

  ImGui::Begin("Cursor");
  {
    auto world_pos = m_.cursor->transform.pos();
    if (ImGui::DragFloat3("World", world_pos.raw_ptr(), -0.1F)) {
      m_.cursor->transform.set_local_pos(world_pos);
    }

    auto ndc_pos = m_.cursor->ndc_pos(*m_.camera);
    if (ImGui::DragFloat2("Screen", ndc_pos.raw_ptr(), -0.01F, -1.F, 1.F)) {
      m_.cursor->set_by_ndc_pos(*m_.camera, ndc_pos);
    }

    if (ImGui::Button("Look at cursor")) {
      m_.camera_gimbal->set_local_pos(m_.cursor->transform.pos());
    }
  }
  ImGui::End();

  ImGui::ShowDemoWindow();

  ImGui::Begin("Scene objects");

  if (ImGui::Button("Add")) {
    ImGui::OpenPopup("AddSceneObjectPopup");
  }

  enum class SceneObjectType : uint8_t {
    Point  = 0,
    Torus  = 1,
    _Count = 2,  // NOLINT
  };

  static constexpr auto kSceneObjectNames = eray::util::StringEnumMapper<SceneObjectType>({
      {SceneObjectType::Point, "Point"},
      {SceneObjectType::Torus, "Torus"},
  });

  if (ImGui::BeginPopup("AddSceneObjectPopup")) {
    for (const auto [type, name] : kSceneObjectNames) {
      if (ImGui::Selectable(name.c_str())) {
        if (type == SceneObjectType::Point) {
          on_scene_object_added(Point{});
        } else {
          on_scene_object_added(Torus{});
        }
      }
    }
    ImGui::EndPopup();
  }

  ImGui::SameLine();

  {
    bool disable = m_.selection.is_empty();
    if (disable) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete")) {
      for (const auto& o : m_.selection) {
        on_scene_object_deleted(o);
      }
      m_.selection.clear();
    }
    if (disable) {
      ImGui::EndDisabled();
    }
  }

  for (const auto& h_p : m_.scene.scene_objs()) {
    if (auto p = m_.scene.get_obj(h_p)) {
      ImGui::PushID(h_p.obj_id);

      bool is_selected = m_.selection.contains(h_p);
      if (ImGui::Selectable(p.value()->name.c_str(), is_selected)) {
        if (is_selected) {
          m_.selection.remove(m_.scene, h_p);
        } else {
          m_.selection.add(m_.scene, h_p);
        }
      }

      ImGui::PopID();
    }
  }

  ImGui::End();

  ImGui::Begin("Selection");
  if (m_.selection.is_single_selection()) {
    if (auto scene_obj = m_.scene.get_obj(m_.selection.first())) {
      ImGui::Text("Name: %s", scene_obj.value()->name.c_str());
      ImGui::SameLine();
      static std::string object_name;
      if (ImGui::Button("Rename")) {
        ImGui::mini::OpenModal("Rename object");
        object_name = scene_obj.value()->name;
      }

      if (ImGui::mini::RenameModal("Rename object", object_name)) {
        scene_obj.value()->name = object_name;
      }

      std::visit(
          eray::util::match{
              [&scene_obj](auto& p) {
                ImGui::Text("Type: %s", p.type_name().c_str());
                ImGui::SameLine();
                ImGui::Text("ID: %d", scene_obj.value()->id());
              },
          },
          scene_obj.value()->object);

      auto world_pos = scene_obj.value()->transform.pos();
      if (ImGui::DragFloat3("World", world_pos.raw_ptr(), -0.1F)) {
        scene_obj.value()->transform.set_local_pos(world_pos);
        scene_obj.value()->mark_dirty();
      }

      std::visit(eray::util::match{
                     [this](Point& p) {
                       ImGui::Text("Part of the lists:");
                       for (const auto& p : p.point_lists()) {
                         if (auto point_list = m_.scene.get_obj(p)) {
                           ImGui::PushID(point_list.value()->id());

                           bool is_selected = m_.selected_point_list_obj && m_.selected_point_list_obj.value() == p;
                           if (ImGui::Selectable(point_list.value()->name.c_str(), is_selected)) {
                             m_.selected_point_list_obj = p;
                           }

                           ImGui::PopID();
                         }
                       }
                     },
                     [&](Torus& t) {
                       if (ImGui::SliderInt("Tess Level X", &t.tess_level.x, kMinTessLevel, kMaxTessLevel)) {
                         scene_obj.value()->mark_dirty();
                       }
                       if (ImGui::SliderInt("Tess Level Y", &t.tess_level.y, kMinTessLevel, kMaxTessLevel)) {
                         scene_obj.value()->mark_dirty();
                       }
                       if (ImGui::SliderFloat("Rad Minor", &t.minor_radius, 0.1F, t.major_radius)) {
                         scene_obj.value()->mark_dirty();
                       }
                       if (ImGui::SliderFloat("Rad Major", &t.major_radius, t.minor_radius, 5.F)) {
                         scene_obj.value()->mark_dirty();
                       }
                     },
                 },
                 scene_obj.value()->object);
    }
  } else if (m_.selection.is_multi_selection()) {
    ImGui::Text("Multiselection");
  } else {
    ImGui::Text("None");
  }
  ImGui::End();

  ImGui::Begin("Point Lists");

  if (ImGui::Button("Add")) {
    on_point_list_object_added();
  }
  ImGui::SameLine();
  {
    bool disable = !m_.selected_point_list_obj;
    if (disable) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete")) {
      on_point_list_object_deleted(*m_.selected_point_list_obj);
      m_.selected_point_list_obj = std::nullopt;
    }
    if (disable) {
      ImGui::EndDisabled();
    }
  }
  for (const auto& h_pl : m_.scene.point_list_objs()) {
    if (auto pl = m_.scene.get_obj(h_pl)) {
      ImGui::PushID(h_pl.obj_id);

      bool is_selected = m_.selected_point_list_obj && m_.selected_point_list_obj.value() == h_pl;
      if (ImGui::Selectable(pl.value()->name.c_str(), is_selected)) {
        m_.selected_point_list_obj = h_pl;
      }

      ImGui::PopID();
    }
  }

  ImGui::End();

  ImGui::Begin("Selected Point List");
  if (m_.selected_point_list_obj) {
    if (auto obj = m_.scene.get_obj(*m_.selected_point_list_obj)) {
      ImGui::Text("Name: %s", obj.value()->name.c_str());
      ImGui::SameLine();
      static std::string object_name;
      if (ImGui::Button("Rename")) {
        ImGui::mini::OpenModal("Rename object");
        object_name = obj.value()->name;
      }

      if (ImGui::mini::RenameModal("Rename object", object_name)) {
        obj.value()->name = object_name;
      }

      ImGui::Text("Points: ");
      auto disable = !m_.selected_point_list_obj || !m_.selection.is_single_selection();
      if (disable) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button("Add")) {
        if (auto p_h = m_.scene.get_point_handle(m_.selection.first())) {
          m_.scene.add_point_to_list(p_h.value(), *m_.selected_point_list_obj);
        }
      }
      if (disable) {
        ImGui::EndDisabled();
      }

      ImGui::SameLine();

      disable = !m_.selection.is_single_selection() || !obj.value()->contains(m_.selection.first());
      if (disable) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button("Remove")) {
        if (auto ph = m_.scene.get_point_handle(m_.selection.first())) {
          m_.scene.remove_point_from_list(*ph, *m_.selected_point_list_obj);
        }
      }
      if (disable) {
        ImGui::EndDisabled();
      }

      for (const auto& p : obj.value()->points()) {
        if (auto p_obj = m_.scene.get_obj(p)) {
          ImGui::PushID(p.obj_id);

          auto o_h         = SceneObjectHandle(p.owner_signature, p.timestamp, p.obj_id);
          bool is_selected = m_.selection.contains(o_h);
          if (ImGui::Selectable(p_obj.value()->name.c_str(), is_selected)) {
            if (is_selected) {
              m_.selection.remove(m_.scene, o_h);
            } else {
              m_.selection.add(m_.scene, o_h);
            }
          }

          ImGui::PopID();
        }
      }
    }
  }

  ImGui::End();
  ImGui::Begin("Tools");
  bool selected = m_.tool_state == ToolState::Cursor;
  if (ImGui::Selectable("Cursor", selected)) {
    on_cursor_state_set();
  }
  selected = m_.tool_state == ToolState::Select;
  if (ImGui::Selectable("Select", selected)) {
    on_select_state_set();
  }
  ImGui::End();

  //   ImGui::mini::gizmo::BeginFrame(ImGui::GetBackgroundDrawList());
  //   ImGuizmo::SetRect(0, 0, window_->size().x, window_->size().y);
  //   static auto m = math::Transform3f();
  //   m.set_local_pos(m_.selection.centroid());
  //   ImGui::mini::gizmo::Transform(m, *m_.camera, ImGui::mini::gizmo::Mode::World,
  //                                 ImGui::mini::gizmo::Operation::Translation);
}

void MiniCadApp::render(Application::Duration /* delta */) {
  m_.scene.visit_dirty_scene_objects([this](SceneObject& obj) {
    std::visit(
        util::match{
            [&](Point&) {
              auto p = obj.transform.pos();
              m_.rs.points_vao.vbo().sub_buffer_data<float>(3 * obj.id(), p.data);
            },
            [&](Torus& t) {
              auto ind  = m_.rs.transferred_torus_ind.at(obj.handle());
              auto mat  = obj.transform.local_to_world_matrix();
              auto r    = math::Vec2f(t.minor_radius, t.major_radius);
              auto tess = t.tess_level;
              m_.rs.torus_vao.vbo("matrices").sub_buffer_data(ind * 20, std::span<float>(r.raw_ptr(), 2));
              m_.rs.torus_vao.vbo("matrices").sub_buffer_data(2 + ind * 20, std::span<int>(tess.raw_ptr(), 2));
              m_.rs.torus_vao.vbo("matrices").sub_buffer_data(4 + ind * 20, std::span<float>(mat[0].raw_ptr(), 4));
              m_.rs.torus_vao.vbo("matrices").sub_buffer_data(4 + ind * 20 + 4, std::span<float>(mat[1].raw_ptr(), 4));
              m_.rs.torus_vao.vbo("matrices").sub_buffer_data(4 + ind * 20 + 8, std::span<float>(mat[2].raw_ptr(), 4));
              m_.rs.torus_vao.vbo("matrices").sub_buffer_data(4 + ind * 20 + 12, std::span<float>(mat[3].raw_ptr(), 4));
            }},
        obj.object);
  });

  m_.scene.visit_dirty_point_objects([this](PointListObject& obj) {
    // TODO(migoox): do it better
    if (!m_.rs.point_list_vaos.contains(obj.handle())) {
      return;
    }
    auto t = obj.points() | std::views::transform([](const PointHandle& ph) { return ph.obj_id; });
    std::vector<PointListObjectId> temp_vec(t.begin(), t.end());
    m_.rs.point_list_vaos.at(obj.handle()).second.buffer_data(std::span{temp_vec}, gl::DataUsage::StaticDraw);
  });

  m_.camera->set_orthographic(m_.use_ortho);

  m_.rs.viewport_fb->bind();
  glPatchParameteri(GL_PATCH_VERTICES, 4);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glClearColor(0.09, 0.05, 0.09, 1.F);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render the Torus
  m_.rs.param_sh_prog->set_uniform("vMat", m_.camera->view_matrix());
  m_.rs.param_sh_prog->set_uniform("pMat", m_.camera->proj_matrix());
  m_.rs.param_sh_prog->bind();
  m_.rs.torus_vao.bind();
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glDrawElementsInstanced(GL_PATCHES, static_cast<GLsizei>(m_.rs.torus_vao.ebo().count()), GL_UNSIGNED_INT, nullptr,
                          m_.rs.transferred_torus_buff.size());

  // Render polylines
  m_.rs.polyline_sh_prog->bind();
  m_.rs.polyline_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
  for (auto& point_list : m_.rs.point_list_vaos) {
    glBindVertexArray(point_list.second.first);
    glDrawElements(GL_LINE_STRIP, point_list.second.second.count(), GL_UNSIGNED_INT, nullptr);
  }

  // Render grid
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  if (m_.grid_on) {
    m_.rs.grid_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
    m_.rs.grid_sh_prog->set_uniform("u_vInvMat", m_.camera->inverse_view_matrix());
    m_.rs.grid_sh_prog->set_uniform("u_camWorldPos", m_.camera->transform.pos());
    m_.rs.grid_sh_prog->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
  }

  // Render points
  glClearColor(0.09, 0.05, 0.09, 1.F);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  m_.rs.viewport_fb->begin_pick_render();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_.rs.point_txt);
  m_.rs.instanced_sprite_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
  m_.rs.instanced_sprite_sh_prog->set_uniform("u_scale", 0.03F);
  m_.rs.instanced_sprite_sh_prog->set_uniform("u_aspectRatio", m_.camera->aspect_ratio());
  m_.rs.instanced_sprite_sh_prog->set_uniform("u_textureSampler", 0);
  m_.rs.instanced_sprite_sh_prog->bind();
  m_.rs.points_vao.bind();
  glDrawElements(GL_POINTS, m_.rs.transferred_points_buff.size(), GL_UNSIGNED_INT, nullptr);
  m_.rs.viewport_fb->end_pick_render();

  // Render centroid
  if (m_.selection.is_multi_selection()) {
    glBindTexture(GL_TEXTURE_2D, m_.rs.centroid_txt);
    m_.rs.sprite_sh_prog->set_uniform("u_worldPos", m_.selection.centroid());
    m_.rs.sprite_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
    m_.rs.sprite_sh_prog->set_uniform("u_aspectRatio", m_.camera->aspect_ratio());
    m_.rs.sprite_sh_prog->set_uniform("u_textureSampler", 0);
    m_.rs.sprite_sh_prog->bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // Render cursor
  glDisable(GL_DEPTH_TEST);
  glBindTexture(GL_TEXTURE_2D, m_.rs.cursor_txt);
  m_.rs.sprite_sh_prog->set_uniform("u_worldPos", m_.cursor->transform.pos());
  m_.rs.sprite_sh_prog->set_uniform("u_pvMat", m_.camera->proj_matrix() * m_.camera->view_matrix());
  m_.rs.sprite_sh_prog->set_uniform("u_aspectRatio", m_.camera->aspect_ratio());
  m_.rs.sprite_sh_prog->set_uniform("u_textureSampler", 0);
  m_.rs.sprite_sh_prog->bind();
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Render to the default framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, m_.rs.viewport_fb->color_texture());
  glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  m_.rs.screen_quad_sh_prog->bind();
  m_.rs.screen_quad_sh_prog->set_uniform("u_textureSampler", 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glEnable(GL_DEPTH_TEST);
}

void MiniCadApp::update(Duration delta) {
  auto deltaf = std::chrono::duration<float>(delta).count();

  if (m_.orbiting_camera_operator.update(*m_.camera, *m_.camera_gimbal, math::Vec2f(window_->mouse_pos()), deltaf)) {
    m_.cursor->mark_dirty();
  }

  m_.cursor->update(*m_.camera, math::Vec2f(window_->mouse_pos_ndc()));
}

bool MiniCadApp::on_scene_object_added(SceneObjectVariant variant) {
  auto obj_handle = m_.scene.create_scene_obj(std::move(variant));
  if (!obj_handle) {
    Logger::err("Could not add a new point list object");
    return false;
  }

  if (auto o = m_.scene.get_obj(*obj_handle)) {
    std::visit(eray::util::match{
                   //
                   [&](Point&) {
                     auto i   = obj_handle->obj_id;
                     auto ind = m_.rs.transferred_points_buff.size();

                     m_.rs.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));

                     m_.rs.transferred_point_ind.insert({*obj_handle, ind});
                     m_.rs.transferred_points_buff.push_back(*obj_handle);

                     o.value()->transform.set_local_pos(m_.cursor->transform.pos());
                   },  //
                   [&](Torus&) {
                     auto ind = m_.rs.transferred_torus_buff.size();

                     m_.rs.transferred_torus_ind.insert({*obj_handle, ind});
                     m_.rs.transferred_torus_buff.push_back(*obj_handle);

                     o.value()->transform.set_local_pos(m_.cursor->transform.pos());
                   }  //
               },
               o.value()->object);
    util::Logger::info("Added scene object \"{}\"", o.value()->name);
    m_.selection.clear();
    m_.selection.add(m_.scene, *obj_handle);
  } else {
    util::Logger::warn("Despite calling create scene object, the object is not added");
  }

  return true;
}

bool MiniCadApp::on_scene_object_deleted(const SceneObjectHandle& handle) {
  std::optional<std::string> obj_name = std::nullopt;
  if (auto o = m_.scene.get_obj(handle)) {
    obj_name    = o.value()->name;
    auto result = std::visit(
        eray::util::match{
            [&](Point&) {
              if (m_.rs.transferred_points_buff.size() - 1 == m_.rs.transferred_point_ind[handle]) {
                m_.rs.transferred_points_buff.pop_back();
                m_.rs.transferred_point_ind.erase(handle);
                return true;
              }

              auto ind = m_.rs.transferred_point_ind[handle];
              m_.rs.transferred_point_ind.erase(handle);
              m_.rs.transferred_points_buff[ind] =
                  m_.rs.transferred_points_buff[m_.rs.transferred_points_buff.size() - 1];
              m_.rs.transferred_point_ind[m_.rs.transferred_points_buff[ind]] = ind;
              m_.rs.transferred_points_buff.pop_back();

              auto i = m_.rs.transferred_points_buff[ind].obj_id;
              m_.rs.points_vao.ebo().sub_buffer_data(ind, std::span<uint32_t>(&i, 1));
              return true;
            },
            [&](Torus&) {
              if (m_.rs.transferred_torus_buff.size() - 1 == m_.rs.transferred_torus_ind.at(handle)) {
                m_.rs.transferred_torus_buff.pop_back();
                m_.rs.transferred_torus_ind.erase(handle);
                if (obj_name) {
                  util::Logger::info("Deleted scene object \"{}\"", *obj_name);
                }
                return true;
              }

              auto ind = m_.rs.transferred_torus_ind[handle];
              m_.rs.transferred_torus_ind.erase(handle);
              m_.rs.transferred_torus_buff[ind] = m_.rs.transferred_torus_buff[m_.rs.transferred_torus_buff.size() - 1];
              m_.rs.transferred_torus_ind[m_.rs.transferred_torus_buff[ind]] = ind;
              m_.rs.transferred_torus_buff.pop_back();

              if (auto o2 = m_.scene.get_obj(m_.rs.transferred_torus_buff[ind])) {
                auto mat = o2.value()->transform.local_to_world_matrix();
                std::visit(
                    eray::util::match{
                        [&](Point&) {},
                        [&](Torus& t) {
                          auto r = math::Vec2f(t.minor_radius, t.major_radius);
                          m_.rs.torus_vao.vbo("matrices").sub_buffer_data(ind * 20, std::span<float>(r.raw_ptr(), 2));
                          auto tess = t.tess_level;
                          m_.rs.torus_vao.vbo("matrices")
                              .sub_buffer_data(2 + ind * 20, std::span<int>(tess.raw_ptr(), 2));
                        },
                    },
                    o2.value()->object);
                m_.rs.torus_vao.vbo("matrices").sub_buffer_data(4 + ind * 20, std::span<float>(mat[0].raw_ptr(), 4));
                m_.rs.torus_vao.vbo("matrices")
                    .sub_buffer_data(4 + ind * 20 + 4, std::span<float>(mat[1].raw_ptr(), 4));
                m_.rs.torus_vao.vbo("matrices")
                    .sub_buffer_data(4 + ind * 20 + 8, std::span<float>(mat[2].raw_ptr(), 4));
                m_.rs.torus_vao.vbo("matrices")
                    .sub_buffer_data(4 + ind * 20 + 12, std::span<float>(mat[3].raw_ptr(), 4));
              }
              return true;
            }},
        o.value()->object);

    m_.scene.delete_obj(handle);
    if (obj_name && result) {
      util::Logger::info("Deleted scene object \"{}\"", *obj_name);
    }
  } else {
    util::Logger::warn("Tried to delete a non existing element");
  }

  return true;
}

bool MiniCadApp::on_point_list_object_added() {
  auto handle = m_.scene.create_list_obj();
  if (!handle) {
    Logger::err("Could not add a new point list object");
    return false;
  }
  m_.selected_point_list_obj = *handle;
  m_.rs.point_list_vaos.insert({*handle, create_point_list_vao(m_.rs.points_vao.vbo())});
  return true;
}

bool MiniCadApp::on_point_list_object_deleted(const PointListObjectHandle& handle) {
  auto id = m_.rs.point_list_vaos.at(handle).first;
  glDeleteVertexArrays(1, &id);
  m_.rs.point_list_vaos.erase(handle);
  m_.scene.delete_obj(handle);
  return true;
}

bool MiniCadApp::on_cursor_state_set() {
  m_.tool_state = ToolState::Cursor;
  return true;
}

bool MiniCadApp::on_select_state_set() {
  m_.tool_state = ToolState::Select;
  return true;
}

bool MiniCadApp::on_tool_action_start() {
  if (m_.tool_state == ToolState::Cursor) {
    m_.cursor->start_movement();
    return true;
  }

  if (m_.tool_state == ToolState::Select) {
    auto m = window_->mouse_pos();
    m_.rs.viewport_fb->bind();
    int id = m_.rs.viewport_fb->sample_mouse_pick(static_cast<size_t>(m.x), static_cast<size_t>(m.y));
    m_.selection.clear();

    if (id < 0) {
      return true;
    }

    if (auto o_h = m_.scene.handle_by_obj_id(static_cast<SceneObjectId>(id))) {
      m_.selection.add(m_.scene, *o_h);
    }
    return true;
  }

  return false;
}

bool MiniCadApp::on_mouse_pressed(const MouseButtonPressedEvent& ev) {
  if (ev.is_on_ui()) {
    return false;
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonLeft) {
    on_tool_action_start();
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonMiddle) {
    m_.orbiting_camera_operator.start_pan(math::Vec2f(ev.x(), ev.y()));
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonRight) {
    m_.orbiting_camera_operator.start_rot(math::Vec2f(ev.x(), ev.y()));
  }

  return true;
}

bool MiniCadApp::on_mouse_released(const MouseButtonReleasedEvent& ev) {
  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonLeft) {
    m_.orbiting_camera_operator.stop_looking_around(*m_.camera, *m_.camera_gimbal);
  }
  m_.cursor->stop_movement();
  m_.orbiting_camera_operator.stop_rot();
  m_.orbiting_camera_operator.stop_pan();
  return true;
}

bool MiniCadApp::on_resize(const WindowResizedEvent& ev) {  // NOLINT
  glViewport(0, 0, ev.width(), ev.height());
  m_.camera->set_aspect_ratio(static_cast<float>(ev.width()) / static_cast<float>(ev.height()));
  m_.camera->recalculate_projection();
  return true;
}

bool MiniCadApp::on_scrolled(const MouseScrolledEvent& ev) {
  m_.orbiting_camera_operator.zoom(*m_.camera, static_cast<float>(ev.y_offset()));
  return true;
}

bool MiniCadApp::on_key_pressed(const eray::os::KeyPressedEvent& ev) {
  if (ev.key_code() == KeyCode::C) {
    m_.camera_gimbal->set_local_pos(math::Vec3f::filled(0.F));
    return true;
  }
  if (ev.key_code() == KeyCode::G) {
    m_.grid_on = !m_.grid_on;
  }
  return false;
}

MiniCadApp::~MiniCadApp() {
  glDeleteTextures(1, &m_.rs.cursor_txt);
  glDeleteTextures(1, &m_.rs.point_txt);
  glDeleteTextures(1, &m_.rs.centroid_txt);
}

}  // namespace mini
