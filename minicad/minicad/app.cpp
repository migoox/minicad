#include <glad/gl.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imguizmo/ImGuizmo.h>

#include <cstdint>
#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
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
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <memory>
#include <minicad/app.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/imgui/modals.hpp>
#include <minicad/imgui/reorder_dnd.hpp>
#include <minicad/imgui/transform.hpp>
#include <minicad/imgui/transform_gizmo.hpp>
#include <minicad/selection/selection.hpp>
#include <minicad/tools/select_tool.hpp>
#include <optional>
#include <ranges>
#include <variant>

namespace mini {

namespace util = eray::util;
namespace math = eray::math;
namespace os   = eray::os;

using System = eray::os::System;
using Logger = eray::util::Logger;

MiniCadApp MiniCadApp::create(std::unique_ptr<os::Window> window) {
  auto camera = std::make_unique<Camera>(false, math::radians(90.F),
                                         static_cast<float>(window->size().x) / static_cast<float>(window->size().y),
                                         0.1F, 1000.F);
  camera->transform.set_local_pos(math::Vec3f(0.F, 0.F, 4.F));

  auto gimbal = std::make_unique<eray::math::Transform3f>();
  camera->transform.set_parent(*gimbal);

  auto assets_path = System::executable_dir() / "assets";

  eray::driver::GLSLShaderManager manager;

  auto centroid_img = util::unwrap_or_panic(eray::res::Image::load_from_path(assets_path / "img" / "centroid.png"));
  auto cursor_img   = util::unwrap_or_panic(eray::res::Image::load_from_path(assets_path / "img" / "cursor.png"));

  auto sr = util::unwrap_or_panic(gl::OpenGLSceneRenderer::create(assets_path, window->size()));
  sr->add_billboard("cursor", cursor_img);
  sr->add_billboard("centroid", centroid_img);

  return MiniCadApp(std::move(window), {
                                           .orbiting_camera_operator = OrbitingCameraOperator(),    //
                                           .cursor                   = std::make_unique<Cursor>(),  //
                                           .camera                   = std::move(camera),           //
                                           .camera_gimbal            = std::move(gimbal),           //
                                           .show_centroid            = false,                       //
                                           .grid_on                  = true,                        //
                                           .use_ortho                = false,                       //
                                           .is_gizmo_used            = false,                       //
                                           .select_tool              = SelectTool(),                //
                                           .scene                    = Scene(std::move(sr)),        //
                                           .tool_state               = ToolState::Cursor,
                                           .selection                = std::make_unique<SceneObjectsSelection>(),    //
                                           .point_list_selection   = std::make_unique<PointListObjectsSelection>(),  //
                                           .helper_point_selection = HelperPointSelection()                          //
                                       });
}

MiniCadApp::MiniCadApp(std::unique_ptr<os::Window> window, Members&& m)
    : Application(std::move(window)), m_(std::move(m)) {
  ImGui::mini::gizmo::SetImGuiContext(ImGui::GetCurrentContext());

  window_->set_event_callback<os::WindowResizedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_resize));
  window_->set_event_callback<os::MouseButtonPressedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_pressed));
  window_->set_event_callback<os::MouseButtonReleasedEvent>(
      class_method_as_event_callback(this, &MiniCadApp::on_mouse_released));
  window_->set_event_callback<os::MouseScrolledEvent>(class_method_as_event_callback(this, &MiniCadApp::on_scrolled));
  window_->set_event_callback<os::KeyPressedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_key_pressed));
}

void MiniCadApp::gui_objects_list_window() {
  ImGui::Begin("Objects");
  if (ImGui::Button("Add")) {
    ImGui::OpenPopup("AddSceneObjectPopup");
  }

  enum class ObjectType : uint8_t {
    Point                   = 0,
    Torus                   = 1,
    Polyline                = 2,
    MultisegmentBezierCurve = 3,
    BSplineCurve            = 4,
    NaturalSplineCurve      = 5,
    _Count                  = 6,  // NOLINT
  };

  enum class PointListObjectType : uint8_t {
    Polyline                = 0,
    MultisegmentBezierCurve = 1,
    BSplineCurve            = 2,
    NaturalSplineCurve      = 3,
    _Count                  = 4,  // NOLINT
  };

  static constexpr auto kSceneObjectNames = eray::util::StringEnumMapper<ObjectType>({
      {ObjectType::Point, "Point"},
      {ObjectType::Torus, "Torus"},
      {ObjectType::Polyline, "Polyline"},
      {ObjectType::MultisegmentBezierCurve, "Bezier"},
      {ObjectType::BSplineCurve, "B-Spline"},
      {ObjectType::NaturalSplineCurve, "Natural Spline"},
  });

  static constexpr auto kPointListObjectNames = eray::util::StringEnumMapper<PointListObjectType>({
      {PointListObjectType::Polyline, "Polyline"},                 //
      {PointListObjectType::MultisegmentBezierCurve, "Bezier"},    //
      {PointListObjectType::BSplineCurve, "B-Spline"},             //
      {PointListObjectType::NaturalSplineCurve, "Natural Spline"}  //
  });

  if (ImGui::BeginPopup("AddSceneObjectPopup")) {
    for (const auto [type, name] : kSceneObjectNames) {
      if (ImGui::Selectable(name.c_str())) {
        switch (type) {
          case ObjectType::Point:
            on_scene_object_created(Point{});
            break;
          case ObjectType::Torus:
            on_scene_object_created(Torus{});
            break;
          case ObjectType::Polyline:
            on_point_list_object_added(Polyline{});
            break;
          case ObjectType::MultisegmentBezierCurve:
            on_point_list_object_added(MultisegmentBezierCurve{});
            break;
          case ObjectType::BSplineCurve:
            on_point_list_object_added(BSplineCurve{});
            break;
          case ObjectType::NaturalSplineCurve:
            on_point_list_object_added(NaturalSplineCurve{});
            break;
          case ObjectType::_Count:
            util::panic("AddSceneObjectPopup failed with object unexpected type");
        }
      }
    }
    ImGui::EndPopup();
  }

  ImGui::SameLine();

  {
    bool disable = m_.selection->is_empty() && m_.point_list_selection->is_empty();
    if (disable) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Delete")) {
      on_selection_deleted();
    }
    if (disable) {
      ImGui::EndDisabled();
    }
  }

  static constexpr zstring_view kPointDragAndDropPayloadType = "PointDragAndDropPayload";
  for (const auto& obj : m_.scene.objs()) {
    std::visit(  //
        util::match{
            [&](const SceneObjectHandle& handle) {
              if (auto o = m_.scene.get_obj(handle)) {
                bool is_selected = m_.selection->contains(handle);
                ImGui::Selectable(o.value()->name.c_str(), is_selected);
                if (ImGui::IsItemHovered()) {
                  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (is_selected) {
                      on_selection_remove(handle);
                    } else {
                      on_selection_add(handle);
                    }
                  }

                  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    on_selection_set_single(handle);
                  }
                }
                if (ImGui::IsItemHovered()) {
                  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("SelectionPopup");
                  }
                }
                if (std::holds_alternative<Point>(o.value()->object)) {
                  if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload(kPointDragAndDropPayloadType.c_str(), &handle, sizeof(SceneObjectHandle));
                    ImGui::Text("%s", o.value()->name.c_str());
                    ImGui::EndDragDropSource();
                  }
                }
              }
            },
            [&](const PointListObjectHandle& handle) {
              if (auto o = m_.scene.get_obj(handle)) {
                bool is_selected = m_.point_list_selection->contains(handle);
                ImGui::Selectable(o.value()->name.c_str(), is_selected);

                if (ImGui::IsItemHovered()) {
                  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    if (is_selected) {
                      on_point_list_selection_remove(handle);
                    } else {
                      on_point_list_selection_add(handle);
                    }
                  }
                  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("SelectionPopup");
                  }
                  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    on_point_list_selection_set_single(handle);
                  }
                }
                if (ImGui::BeginDragDropTarget()) {
                  if (const ImGuiPayload* payload =
                          ImGui::AcceptDragDropPayload(kPointDragAndDropPayloadType.c_str())) {
                    IM_ASSERT(payload->DataSize == sizeof(SceneObjectHandle));
                    auto point_handle = *static_cast<SceneObjectHandle*>(payload->Data);
                    m_.scene.add_to_list(point_handle, handle);
                  }
                  ImGui::EndDragDropTarget();
                }
              }
            },
        },
        obj);
  }

  if (ImGui::BeginPopup("SelectionPopup")) {
    bool disabled = !m_.selection->is_points_only() || !m_.point_list_selection->is_empty();
    if (disabled) {
      ImGui::BeginDisabled();
    }
    if (ImGui::BeginMenu("Create Point List")) {
      for (const auto [type, name] : kPointListObjectNames) {
        if (ImGui::Selectable(name.c_str())) {
          switch (type) {
            case PointListObjectType::Polyline:
              on_point_list_object_added_from_points_selection(Polyline{});
              break;
            case PointListObjectType::MultisegmentBezierCurve:
              on_point_list_object_added_from_points_selection(MultisegmentBezierCurve{});
              break;
            case PointListObjectType::BSplineCurve:
              on_point_list_object_added_from_points_selection(BSplineCurve{});
              break;
            case PointListObjectType::NaturalSplineCurve:
              on_point_list_object_added_from_points_selection(NaturalSplineCurve{});
              break;
            case PointListObjectType::_Count:
              util::panic("AddPointListObjectPopup failed with object unexpected type");
          }
        }
      }
      ImGui::EndMenu();
    }
    if (disabled) {
      ImGui::EndDisabled();
    }
    ImGui::Separator();

    if (ImGui::Selectable("Delete")) {
      on_selection_deleted();
    }

    ImGui::EndPopup();
  }

  ImGui::End();
}

void MiniCadApp::gui_selection_window() {
  ImGui::Begin("Selection");
  if (m_.selection->is_single_selection()) {
    if (auto scene_obj = m_.scene.get_obj(m_.selection->first())) {
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

      ImGui::mini::Transform(scene_obj.value()->transform, [&]() { scene_obj.value()->update(); });

      std::visit(eray::util::match{
                     [](Point& /*p*/) {
                       //    ImGui::Text("Part of the lists:");
                       //    for (const auto& p : p()) {
                       //      if (auto point_list = m_.scene.get_obj(p)) {
                       //        ImGui::PushID(point_list.value()->id());

                       //        bool is_selected = m_.point_list_selection->contains(point_list.value()->handle());
                       //        if (ImGui::Selectable(point_list.value()->name.c_str(), is_selected)) {
                       //          m_.point_list_selection->add(point_list.value()->handle());
                       //        }

                       //        ImGui::PopID();
                       //      }
                       //    }
                     },
                     [&](Torus& t) {
                       if (ImGui::SliderInt("Tess Level X", &t.tess_level.x, kMinTessLevel, kMaxTessLevel)) {
                         scene_obj.value()->update();
                       }
                       if (ImGui::SliderInt("Tess Level Y", &t.tess_level.y, kMinTessLevel, kMaxTessLevel)) {
                         scene_obj.value()->update();
                       }
                       if (ImGui::SliderFloat("Rad Minor", &t.minor_radius, 0.1F, t.major_radius)) {
                         scene_obj.value()->update();
                       }
                       if (ImGui::SliderFloat("Rad Major", &t.major_radius, t.minor_radius, 5.F)) {
                         scene_obj.value()->update();
                       }
                     },
                 },
                 scene_obj.value()->object);
    }
  } else if (m_.selection->is_multi_selection()) {
    ImGui::Text("Multiselection");
    ImGui::mini::Transform(m_.selection->transform, [&]() { m_.selection->update_transforms(m_.scene, *m_.cursor); });
  } else {
    ImGui::Text("None");
  }
  ImGui::End();
}

void MiniCadApp::gui_point_list_window() {
  ImGui::Begin("Selected Point List");
  if (m_.point_list_selection->is_single_selection()) {
    static const zstring_view kReorderPointsDragAndDropPayloadType = "ReorderPointsDragAndDropPayload";
    auto reorder_dnd = ImGui::mini::ReorderDnD(kReorderPointsDragAndDropPayloadType);

    if (auto obj = m_.scene.get_obj(m_.point_list_selection->first())) {
      auto type_name = std::visit(util::match{[](auto& obj) { return obj.type_name(); }}, obj.value()->object);

      ImGui::Text("Type: %s", type_name.c_str());
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

      if (!std::holds_alternative<Polyline>(obj.value()->object)) {
        auto state = m_.scene.renderer().object_rs(obj.value()->handle());
        if (state) {
          if (ImGui::Checkbox("Polyline", &state->show_polyline)) {
            m_.scene.renderer().set_object_rs(obj.value()->handle(), *state);
          }
        }
      }

      if (ImGui::BeginTabBar("PointsTabBar", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Control Points")) {
          m_.selection->is_single_selection();

          if (ImGui::Button("Add")) {
            on_point_created_in_point_list(m_.point_list_selection->first());
          }
          ImGui::SameLine();
          auto disable = !m_.selection->is_single_selection() || !obj.value()->contains(m_.selection->first());
          if (disable) {
            ImGui::BeginDisabled();
          }
          if (ImGui::Button("Remove")) {
            m_.scene.remove_from_list(m_.selection->first(), m_.point_list_selection->first());
          }
          if (disable) {
            ImGui::EndDisabled();
          }

          for (auto& p : obj.value()->point_objects()) {
            ImGui::PushID(static_cast<int>(p.id()));

            bool is_selected = m_.selection->contains(p.handle());
            if (ImGui::Selectable(p.name.c_str(), is_selected)) {
              if (is_selected) {
                on_selection_remove(p.handle());
              } else {
                on_selection_add(p.handle());
              }
            }
            reorder_dnd.drag_and_drop_component(p, true);

            ImGui::PopID();
          }
          ImGui::EndTabItem();
        }
        if (std::holds_alternative<BSplineCurve>(obj.value()->object) && ImGui::BeginTabItem("Bernstein Points")) {
          auto id = 0;
          ImGui::Separator();
          for (const auto& points : std::get<BSplineCurve>(obj.value()->object).bernstein_points() |
                                        std::views::slide(4) | std::views::stride(4)) {
            for (auto p : points) {
              ImGui::PushID(id++);
              ImGui::InputFloat3("##Point", p.data, "%.3f", ImGuiInputTextFlags_ReadOnly);
              ImGui::PopID();
            }
            ImGui::Separator();
          }
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    on_points_reorder(m_.point_list_selection->first(), reorder_dnd.source, reorder_dnd.before_dest,
                      reorder_dnd.after_dest);
  }
  ImGui::End();
}

void MiniCadApp::render_gui(Duration /* delta */) {
  ImGui::ShowDemoWindow();

  ImGui::Begin("MiNI CAD");
  {
    ImGui::Text("FPS: %d", fps_);
    ImGui::Checkbox("Grid", &m_.grid_on);
    ImGui::Checkbox("Ortho", &m_.use_ortho);
    if (ImGui::Button("Look at cursor")) {
      m_.camera_gimbal->set_local_pos(m_.cursor->transform.pos());
    }
    if (ImGui::Button("Look at origin")) {
      m_.camera_gimbal->set_local_pos(m_.selection->centroid());
    }
  }
  ImGui::End();

  gui_objects_list_window();
  gui_selection_window();
  gui_point_list_window();

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

  static auto operation = ImGui::mini::gizmo::Operation::Translation;
  static auto mode      = ImGui::mini::gizmo::Mode::World;
  ImGui::Begin("Tool");
  {
    if (m_.tool_state == ToolState::Cursor) {
      auto world_pos = m_.cursor->transform.pos();
      if (ImGui::DragFloat3("World", world_pos.raw_ptr(), -0.1F)) {
        m_.cursor->transform.set_local_pos(world_pos);
      }

      auto ndc_pos = m_.cursor->ndc_pos(*m_.camera);
      if (ImGui::DragFloat2("Screen", ndc_pos.raw_ptr(), -0.01F, -1.F, 1.F)) {
        m_.cursor->set_by_ndc_pos(*m_.camera, ndc_pos);
      }

    } else if (m_.tool_state == ToolState::Select) {
      static constexpr auto kOperationName = util::StringEnumMapper<ImGui::mini::gizmo::Operation>({
          {ImGui::mini::gizmo::Operation::Translation, "Translate"},
          {ImGui::mini::gizmo::Operation::Rotation, "Rotate"},
          {ImGui::mini::gizmo::Operation::Scale, "Scale"},
      });
      if (ImGui::BeginCombo("Operation", kOperationName[operation].c_str())) {
        for (auto [op, name] : kOperationName) {
          const auto is_selected = (operation == op);
          if (ImGui::Selectable(name.c_str(), is_selected)) {
            operation = op;
          }

          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }

      static constexpr auto kModeName = util::StringEnumMapper<ImGui::mini::gizmo::Mode>({
          {ImGui::mini::gizmo::Mode::Local, "Local"},
          {ImGui::mini::gizmo::Mode::World, "World"},
      });
      if (ImGui::BeginCombo("Mode", kModeName[mode].c_str())) {
        for (auto [m, name] : kModeName) {
          const auto is_selected = (mode == m);
          if (ImGui::Selectable(name.c_str(), is_selected)) {
            mode = m;
          }

          if (is_selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      bool cursor_as_origin = m_.selection->is_using_custom_origin();
      if (ImGui::Checkbox("Use cursor as origin", &cursor_as_origin)) {
        m_.selection->use_custom_origin(m_.scene, cursor_as_origin);
        if (cursor_as_origin) {
          m_.selection->set_custom_origin(m_.scene, m_.cursor->transform.pos());
        }
      }
    }
  }
  ImGui::End();

  if (m_.tool_state == ToolState::Select && m_.select_tool.is_box_select_active()) {
    auto rect   = m_.select_tool.box(math::Vec2f(window_->mouse_pos()));
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##BoxSelect", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(
        ImVec2(static_cast<float>(rect.pos.x), static_cast<float>(rect.pos.y)),
        ImVec2(static_cast<float>(rect.pos.x + rect.size.x), static_cast<float>(rect.pos.y + rect.size.y)),
        IM_COL32(56, 123, 203, 255), 0.0F, ImDrawFlags_None, 2.0F);

    ImGui::End();
  }

  if (m_.tool_state == ToolState::Select) {
    ImGui::mini::gizmo::BeginFrame(ImGui::GetBackgroundDrawList());
    ImGui::mini::gizmo::SetRect(0, 0, math::Vec2f(window_->size()));

    if (m_.selection->is_multi_selection() || m_.selection->is_using_custom_origin()) {
      if (ImGui::mini::gizmo::Transform(m_.selection->transform, *m_.camera, mode, operation,
                                        [&]() { m_.selection->update_transforms(m_.scene, *m_.cursor); })) {
        m_.select_tool.end_box_select();
      }
    } else if (m_.selection->is_single_selection()) {
      if (auto o = m_.scene.get_obj(m_.selection->first())) {
        if (ImGui::mini::gizmo::Transform(o.value()->transform, *m_.camera, mode, operation,
                                          [&]() { o.value()->update(); })) {
          m_.select_tool.end_box_select();
        }
      }
    }

    if (m_.helper_point_selection.is_selected()) {
      auto p = *m_.helper_point_selection.pos(m_.scene);
      if (ImGui::mini::gizmo::Translation(p, *m_.camera)) {
        m_.helper_point_selection.set_point(m_.scene, p);
        m_.select_tool.end_box_select();
      }
    }
  }
}

void MiniCadApp::render(Application::Duration /* delta */) {
  m_.camera->set_orthographic(m_.use_ortho);

  m_.scene.renderer().update(m_.scene);

  m_.scene.renderer().show_grid(m_.grid_on);
  m_.scene.renderer().billboard("cursor").position   = m_.cursor->transform.pos();
  m_.scene.renderer().billboard("centroid").show     = m_.selection->is_multi_selection();
  m_.scene.renderer().billboard("centroid").position = m_.selection->centroid();
  m_.scene.renderer().render(*m_.camera);
}

void MiniCadApp::update(Duration delta) {
  auto deltaf = std::chrono::duration<float>(delta).count();

  if (m_.orbiting_camera_operator.update(*m_.camera, *m_.camera_gimbal, math::Vec2f(window_->mouse_pos()), deltaf)) {
    m_.cursor->mark_dirty();
  }

  m_.cursor->update(*m_.camera, math::Vec2f(window_->mouse_pos_ndc()));
}

bool MiniCadApp::on_scene_object_created(SceneObjectVariant variant) {
  auto obj_handle = m_.scene.create_scene_obj(std::move(variant));
  if (!obj_handle) {
    Logger::err("Could not add a new point list object");
    return false;
  }

  if (auto o = m_.scene.get_obj(*obj_handle)) {
    o.value()->transform.set_local_pos(m_.cursor->transform.pos());
    o.value()->update();
    on_selection_clear();
    on_selection_add(*obj_handle);
    util::Logger::info("Created scene object \"{}\"", o.value()->name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_point_created_in_point_list(const PointListObjectHandle& handle) {
  auto obj_handle = m_.scene.create_scene_obj(Point{});
  if (!obj_handle) {
    Logger::err("Could not add a new point list object");
    return false;
  }

  if (auto o = m_.scene.get_obj(*obj_handle)) {
    m_.scene.add_to_list(*obj_handle, handle);
    o.value()->transform.set_local_pos(m_.cursor->transform.pos());
    o.value()->update();
    on_selection_clear();
    on_selection_add(*obj_handle);
    util::Logger::info("Created scene object \"{}\"", o.value()->name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_scene_object_deleted(const SceneObjectHandle& handle) {
  if (auto o = m_.scene.get_obj(handle)) {
    m_.scene.delete_obj(handle);
    util::Logger::info("Deleted scene object \"{}\"", o.value()->name);
    m_.scene.renderer().push_object_rs_cmd(SceneObjectRSCommand(
        o.value()->handle(), SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Visible)));
  } else {
    util::Logger::warn("Tried to delete a non existing element");
  }

  return true;
}

bool MiniCadApp::on_point_list_object_added(PointListObjectVariant variant) {
  if (auto handle = m_.scene.create_list_obj(std::move(variant))) {
    m_.point_list_selection->add(*handle);
    if (auto o = m_.scene.get_obj(*handle)) {
      Logger::info("Created point list object \"{}\"", o.value()->name);
    }
    return true;
  }

  return false;
}

bool MiniCadApp::on_point_list_object_added_from_points_selection(PointListObjectVariant variant) {
  if (auto handle = m_.scene.create_list_obj(std::move(variant))) {
    m_.point_list_selection->add(*handle);
    if (auto o = m_.scene.get_obj(*handle)) {
      Logger::info("Created point list object \"{}\"", o.value()->name);

      if (m_.selection->is_points_only()) {
        for (const auto& h : *m_.selection) {
          if (!o.value()->add(h)) {
            Logger::warn("Could not add scene object with id to point list\"{}\"", h.obj_id);
          }
        }
      }
    }

    return true;
  }

  return false;
}

bool MiniCadApp::on_point_list_object_deleted(const PointListObjectHandle& handle) {
  if (auto o = m_.scene.get_obj(handle)) {
    Logger::info("Deleted point list object \"{}\"", o.value()->name);
    m_.scene.delete_obj(handle);
    return true;
  }

  return false;
}

bool MiniCadApp::on_points_reorder(const PointListObjectHandle& handle, const std::optional<SceneObjectHandle>& source,
                                   const std::optional<SceneObjectHandle>& before_dest,
                                   const std::optional<SceneObjectHandle>& after_dest) {
  if (auto obj = m_.scene.get_obj(handle)) {
    if (source && before_dest) {
      if (!obj.value()->move_before(*before_dest, *source)) {
        util::Logger::warn("Could not reorder the points in point list {}.", obj.value()->name);
      }
      obj.value()->update();
      return true;
    }

    if (source && after_dest) {
      if (!obj.value()->move_after(*after_dest, *source)) {
        util::Logger::warn("Could not reorder the points in point list {}.", obj.value()->name);
      }
      obj.value()->update();
      return true;
    }
  }

  return false;
}

bool MiniCadApp::on_selection_add(const SceneObjectHandle& handle) {
  m_.selection->add(m_.scene, handle);
  m_.scene.renderer().push_object_rs_cmd(
      SceneObjectRSCommand(handle, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Selected)));

  return true;
}

bool MiniCadApp::on_selection_remove(const SceneObjectHandle& handle) {
  m_.selection->remove(m_.scene, handle);
  m_.scene.renderer().push_object_rs_cmd(
      SceneObjectRSCommand(handle, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Visible)));
  return true;
}

bool MiniCadApp::on_selection_set_single(const SceneObjectHandle& handle) {
  on_point_list_selection_clear();
  on_selection_clear();
  on_selection_add(handle);
  return true;
}

bool MiniCadApp::on_selection_clear() {
  for (const auto& handle : *m_.selection) {
    m_.scene.renderer().push_object_rs_cmd(
        SceneObjectRSCommand(handle, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Visible)));
  }
  m_.selection->clear(m_.scene);
  m_.helper_point_selection.clear();
  return true;
}

bool MiniCadApp::on_point_list_selection_add(const PointListObjectHandle& handle) {
  if (auto o = m_.scene.get_obj(handle)) {
    auto handles = o.value()->point_handles();
    on_selection_add_many(handles.begin(), handles.end());
  }
  m_.point_list_selection->add(handle);
  return true;
}

bool MiniCadApp::on_point_list_selection_set_single(const PointListObjectHandle& handle) {
  on_point_list_selection_clear();
  on_selection_clear();
  m_.point_list_selection->add(handle);
  return true;
}

bool MiniCadApp::on_point_list_selection_remove(const PointListObjectHandle& handle) {
  if (auto o = m_.scene.get_obj(handle)) {
    auto handles = o.value()->point_handles();
    on_selection_remove_many(handles.begin(), handles.end());
  }
  m_.point_list_selection->remove(handle);
  return true;
}

bool MiniCadApp::on_point_list_selection_clear() {
  for (const auto& pl : *m_.point_list_selection) {
    if (auto o = m_.scene.get_obj(pl)) {
      auto handles = o.value()->point_handles();
      on_selection_remove_many(handles.begin(), handles.end());
    }
  }
  m_.point_list_selection->clear();
  return true;
}

bool MiniCadApp::on_selection_deleted() {
  auto result = false;
  for (const auto& o : *m_.selection) {
    result = on_scene_object_deleted(o) || result;
  }
  for (const auto& o : *m_.point_list_selection) {
    result = on_point_list_object_deleted(o) || result;
  }
  on_selection_clear();
  on_point_list_selection_clear();

  return result;
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
    if (ImGui::mini::gizmo::IsOverTransform()) {
      m_.is_gizmo_used = true;
    }
    if (!m_.is_gizmo_used) {
      m_.select_tool.start_box_select(math::Vec2f(window_->mouse_pos()));
    }
    return true;
  }

  return false;
}

bool MiniCadApp::on_tool_action_end() {
  if (m_.tool_state == ToolState::Select && m_.select_tool.is_box_select_active()) {
    m_.select_tool.end_box_select();
    auto box = m_.select_tool.box(math::Vec2f(window_->mouse_pos()));

    auto ids =
        m_.scene.renderer().sample_mouse_pick_box(static_cast<size_t>(box.pos.x), static_cast<size_t>(box.pos.y),
                                                  static_cast<size_t>(box.size.x), static_cast<size_t>(box.size.y));
    on_selection_clear();
    on_point_list_selection_clear();

    if (ids.size() == 1) {
      int point_list_id   = 0;
      int helper_point_id = 0;
      auto id             = *ids.begin();
      if (id & (1 << 31)) {
        point_list_id   = (id >> 18) & 0x1FFF;
        helper_point_id = id & 0x3FFFF;

        if (auto h = m_.scene.handle_by_point_list_obj_id(static_cast<PointListObjectId>(point_list_id))) {
          if (auto o = m_.scene.get_obj(*h)) {
            m_.helper_point_selection.set_selection(
                HelperPoint{.parent = *h, .helper_point = static_cast<size_t>(helper_point_id)});
          }
        }
        return true;
      }
    }

    if (ids.empty()) {
      return true;
    }

    auto handles = std::vector<SceneObjectHandle>();
    for (auto id : ids) {
      if (auto h = m_.scene.handle_by_scene_obj_id(static_cast<SceneObjectId>(id))) {
        handles.push_back(*h);
      }
    }
    on_selection_add_many(handles.begin(), handles.end());
    return true;
  }

  return false;
}

bool MiniCadApp::on_mouse_pressed(const os::MouseButtonPressedEvent& ev) {
  if (ev.is_on_ui() || m_.is_gizmo_used) {
    return false;
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonLeft) {
    on_tool_action_start();
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonMiddle) {
    m_.orbiting_camera_operator.start_pan(math::Vec2f(static_cast<float>(ev.x()), static_cast<float>(ev.y())));
  }

  if (ev.mouse_btn_code() == eray::os::MouseBtnCode::MouseButtonRight) {
    m_.orbiting_camera_operator.start_rot(math::Vec2f(static_cast<float>(ev.x()), static_cast<float>(ev.y())));
  }

  return true;
}

bool MiniCadApp::on_mouse_released(const os::MouseButtonReleasedEvent& ev) {
  if (ev.mouse_btn_code() == os::MouseBtnCode::MouseButtonLeft) {
    m_.orbiting_camera_operator.stop_looking_around(*m_.camera, *m_.camera_gimbal);
  }
  m_.cursor->stop_movement();
  m_.orbiting_camera_operator.stop_rot();
  m_.orbiting_camera_operator.stop_pan();
  m_.is_gizmo_used = false;
  if (ev.is_on_ui()) {
    return true;
  }

  if (ev.mouse_btn_code() == os::MouseBtnCode::MouseButtonLeft) {
    on_tool_action_end();
  }
  return true;
}

bool MiniCadApp::on_resize(const os::WindowResizedEvent& ev) {  // NOLINT
  m_.scene.renderer().resize_viewport(math::Vec2i(ev.width(), ev.height()));
  m_.camera->set_aspect_ratio(static_cast<float>(ev.width()) / static_cast<float>(ev.height()));
  m_.camera->recalculate_projection();
  return true;
}

bool MiniCadApp::on_scrolled(const os::MouseScrolledEvent& ev) {
  m_.orbiting_camera_operator.zoom(*m_.camera, static_cast<float>(ev.y_offset()));
  return true;
}

bool MiniCadApp::on_key_pressed(const eray::os::KeyPressedEvent& ev) {
  if (ev.key_code() == os::KeyCode::C) {
    m_.camera_gimbal->set_local_pos(math::Vec3f::filled(0.F));
    return true;
  }
  if (ev.key_code() == os::KeyCode::G) {
    m_.grid_on = !m_.grid_on;
  }
  return false;
}

MiniCadApp::~MiniCadApp() {}

}  // namespace mini
