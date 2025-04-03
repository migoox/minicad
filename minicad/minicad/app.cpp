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
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <minicad/app.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/imgui/modals.hpp>
#include <minicad/imgui/transform_gizmo.hpp>
#include <optional>
#include <variant>

namespace mini {

// NOLINTBEGIN
using namespace eray;
using namespace eray::driver;
using namespace eray::os;
using namespace eray::util;
// NOLINTEND

MiniCadApp MiniCadApp::create(std::unique_ptr<Window> window) {
  auto camera = std::make_unique<Camera>(false, math::radians(90.F),
                                         static_cast<float>(window->size().x) / static_cast<float>(window->size().y),
                                         0.1F, 1000.F);
  camera->transform.set_local_pos(math::Vec3f(0.F, 0.F, 4.F));

  auto gimbal = std::make_unique<eray::math::Transform3f>();
  camera->transform.set_parent(*gimbal);

  auto assets_path = System::executable_dir() / "assets";

  GLSLShaderManager manager;
  auto screen_quad_vert = unwrap_or_panic(manager.load_shader(assets_path / "screen_quad.vert"));
  auto screen_quad_frag = unwrap_or_panic(manager.load_shader(assets_path / "screen_quad.frag"));
  auto screen_quad_prog = unwrap_or_panic(gl::RenderingShaderProgram::create(
      "screen_quad_shader", std::move(screen_quad_vert), std::move(screen_quad_frag)));

  auto win_size = math::Vec2f(static_cast<float>(window->size().x), static_cast<float>(window->size().y));

  auto sr           = unwrap_or_panic(SceneRenderer::create(assets_path));
  auto centroid_img = unwrap_or_panic(eray::res::Image::load_from_path(assets_path / "centroid.png"));
  auto cursor_img   = unwrap_or_panic(eray::res::Image::load_from_path(assets_path / "cursor.png"));

  sr.add_billboard("cursor", cursor_img);
  sr.add_billboard("centroid", centroid_img);

  return MiniCadApp(std::move(window),
                    {
                        .cursor              = std::make_unique<Cursor>(),  //
                        .camera              = std::move(camera),           //
                        .camera_gimbal       = std::move(gimbal),           //
                        .grid_on             = true,                        //
                        .scene               = Scene(),                     //
                        .scene_renderer      = std::move(sr),
                        .screen_quad_sh_prog = std::move(screen_quad_prog),
                        .viewport_fb         = std::make_unique<gl::ViewportFramebuffer>(win_size.x,
                                                                                         win_size.y),  //
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

  //   auto view = m_.camera->view_matrix();
  //   auto proj = m_.camera->proj_matrix();
  //   auto id   = eray::math::Mat4f::identity();
  //   ImGui::mini::gizmo::BeginFrame(ImGui::GetBackgroundDrawList());
  //   ImGuizmo::SetRect(0, 0, window_->size().x, window_->size().y);
  //   ImGuizmo::DrawCubes(view.raw_ptr(), proj.raw_ptr(), id.raw_ptr(), 1);
}

void MiniCadApp::render(Application::Duration /* delta */) {
  m_.scene.visit_dirty_scene_objects([this](SceneObject& obj) { m_.scene_renderer.update_scene_object(obj); });
  m_.scene.visit_dirty_point_objects([this](PointListObject& obj) { m_.scene_renderer.update_point_list_object(obj); });

  m_.camera->set_orthographic(m_.use_ortho);

  m_.scene_renderer.show_grid(m_.grid_on);
  m_.scene_renderer.billboard("cursor").position   = m_.cursor->transform.pos();
  m_.scene_renderer.billboard("centroid").show     = m_.selection.is_multi_selection();
  m_.scene_renderer.billboard("centroid").position = m_.selection.centroid();
  m_.scene_renderer.render(*m_.viewport_fb, *m_.camera);

  // Render to the default framebuffer
  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, m_.viewport_fb->color_texture());
  glClearColor(1.0F, 1.0F, 1.0F, 1.0F);
  glClear(GL_COLOR_BUFFER_BIT);
  m_.screen_quad_sh_prog->bind();
  m_.screen_quad_sh_prog->set_uniform("u_textureSampler", 0);
  glDrawArrays(GL_TRIANGLES, 0, 6);
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
    m_.scene_renderer.add_scene_object(*o.value());
    o.value()->transform.set_local_pos(m_.cursor->transform.pos());
    m_.selection.clear();
    m_.selection.add(m_.scene, *obj_handle);
    util::Logger::info("Added scene object \"{}\"", o.value()->name);
  } else {
    util::Logger::warn("Despite calling create scene object, the object has not been added");
  }

  return true;
}

bool MiniCadApp::on_scene_object_deleted(const SceneObjectHandle& handle) {
  if (auto o = m_.scene.get_obj(handle)) {
    m_.scene_renderer.delete_scene_object(*o.value(), m_.scene);
    util::Logger::info("Deleted scene object \"{}\"", o.value()->name);
    m_.scene.delete_obj(handle);
  } else {
    util::Logger::warn("Tried to delete a non existing element");
  }

  return true;
}

bool MiniCadApp::on_point_list_object_added() {
  if (auto handle = m_.scene.create_list_obj()) {
    m_.selected_point_list_obj = *handle;
    if (auto o = m_.scene.get_obj(*handle)) {
      m_.scene_renderer.add_point_list_object(*o.value());
      Logger::info("Added point list object \"{}\"", o.value()->name);
    }
    return true;
  }

  return false;
}

bool MiniCadApp::on_point_list_object_deleted(const PointListObjectHandle& handle) {
  if (auto o = m_.scene.get_obj(handle)) {
    m_.scene_renderer.delete_point_list_object(*o.value());
    Logger::info("Deleted point list object \"{}\"", o.value()->name);
    m_.scene.delete_obj(handle);
    return true;
  }

  return false;
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
    m_.viewport_fb->bind();
    int id = m_.viewport_fb->sample_mouse_pick(static_cast<size_t>(m.x), static_cast<size_t>(m.y));
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
  m_.viewport_fb->resize(ev.width(), ev.height());
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

MiniCadApp::~MiniCadApp() {}

}  // namespace mini
