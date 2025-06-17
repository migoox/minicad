#include <glad/gl.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imguizmo/ImGuizmo.h>

#include <cstdint>
#include <fstream>
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
#include <liberay/util/object_handle.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/timer.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/algorithm/hole_finder.hpp>
#include <libminicad/renderer/gl/opengl_scene_renderer.hpp>
#include <libminicad/renderer/rendering_command.hpp>
#include <libminicad/renderer/scene_renderer.hpp>
#include <libminicad/renderer/visibility_state.hpp>
#include <libminicad/scene/fill_in_suface.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <libminicad/serialization/json/json.hpp>
#include <memory>
#include <minicad/app.hpp>
#include <minicad/camera/orbiting_camera_operator.hpp>
#include <minicad/fonts/font_awesome.hpp>
#include <minicad/gui_components/object_list.hpp>
#include <minicad/imgui/modals.hpp>
#include <minicad/imgui/reorder_dnd.hpp>
#include <minicad/imgui/transform.hpp>
#include <minicad/imgui/transform_gizmo.hpp>
#include <minicad/selection/selection.hpp>
#include <minicad/tools/select_tool.hpp>
#include <optional>
#include <ranges>
#include <tracy/Tracy.hpp>
#include <variant>

namespace mini {

namespace util = eray::util;
namespace math = eray::math;
namespace os   = eray::os;

using System = eray::os::System;
using Logger = eray::util::Logger;

template <typename T>
using match = eray::util::match<T>;

MiniCadApp MiniCadApp::create(std::unique_ptr<os::Window> window) {
  // Setup fonts
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontDefault();
  // 13.0f is the size of the default font. Change to the font size you use.
  float base_font_size = 20.0F;
  // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
  float icon_font_size = base_font_size * 2.0F / 3.0F;

  // Merge in icons from Font Awesome
  static const ImWchar kIconsRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
  ImFontConfig icons_config;
  icons_config.MergeMode        = true;
  icons_config.PixelSnapH       = true;
  icons_config.GlyphMinAdvanceX = icon_font_size;
  auto p = System::path_to_utf8str(System::executable_dir() / "assets" / "fonts" / "fa-solid-900.ttf");
  io.Fonts->AddFontFromFileTTF(p.c_str(), icon_font_size, &icons_config, kIconsRanges);
  // use FONT_ICON_FILE_NAME_FAR if you want regular instead of solid

  auto camera = std::make_unique<Camera>(false, math::radians(60.F),
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
                                           .proj_path                = std::nullopt,
                                           .orbiting_camera_operator = OrbitingCameraOperator(),    //
                                           .cursor                   = std::make_unique<Cursor>(),  //
                                           .camera                   = std::move(camera),           //
                                           .camera_gimbal            = std::move(gimbal),           //
                                           .show_centroid            = false,                       //
                                           .grid_on                  = true,                        //
                                           .use_ortho                = false,                       //
                                           .is_gizmo_used            = false,                       //
                                           .ctrl_pressed             = false,                       //
                                           .select_tool              = SelectTool(),                //
                                           .scene                    = Scene(std::move(sr)),        //
                                           .tool_state               = ToolState::Cursor,
                                           .scene_obj_selection      = std::make_unique<SceneObjectsSelection>(),    //
                                           .non_scene_obj_selection  = std::make_unique<NonSceneObjectSelection>(),  //
                                           .helper_point_selection   = HelperPointSelection()                        //
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
  window_->set_event_callback<os::KeyReleasedEvent>(class_method_as_event_callback(this, &MiniCadApp::on_key_released));
}

void MiniCadApp::gui_objects_list_window() {
  enum class CurveType : uint8_t {
    Polyline                = 0,
    MultisegmentBezierCurve = 1,
    BSplineCurve            = 2,
    NaturalSplineCurve      = 3,
    _Count                  = 4,  // NOLINT
  };

  enum class PatchSurfaceType : uint8_t {
    BezierPatchSurface = 0,
    BPatchSurface      = 1,
    _Count             = 2,  // NOLINT
  };

  static constexpr auto kCurveNames = eray::util::StringEnumMapper<CurveType>({
      {CurveType::Polyline, "Polyline"},                       //
      {CurveType::MultisegmentBezierCurve, "Bezier Curve"},    //
      {CurveType::BSplineCurve, "B-Spline Curve"},             //
      {CurveType::NaturalSplineCurve, "Natural Spline Curve"}  //
  });

  static constexpr auto kPatchSurfaceNames = eray::util::StringEnumMapper<PatchSurfaceType>({
      {PatchSurfaceType::BezierPatchSurface, "Bezier Patch Surface"},  //
      {PatchSurfaceType::BPatchSurface, "B-Patch Surface"},            //
  });
  ImGui::Begin("Objects");
  if (ImGui::Button(ICON_FA_CIRCLE " Point")) {
    on_scene_object_added(Point{});
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_BEZIER_CURVE " Curve")) {
    ImGui::OpenPopup("AddCurvePopup");
  }
  if (ImGui::Button(ICON_FA_SOLAR_PANEL " Patches")) {
    ImGui::OpenPopup("AddPatchesPopup");
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_FA_GLOBE " Param Surface")) {
    on_scene_object_added(Torus{});
  }

  if (ImGui::BeginPopup("AddCurvePopup")) {
    for (const auto [type, name] : kCurveNames) {
      if (ImGui::Selectable(name.c_str())) {
        switch (type) {
          case CurveType::Polyline:
            on_curve_added(Polyline{});
            break;
          case CurveType::MultisegmentBezierCurve:
            on_curve_added(MultisegmentBezierCurve{});
            break;
          case CurveType::BSplineCurve:
            on_curve_added(BSplineCurve{});
            break;
          case CurveType::NaturalSplineCurve:
            on_curve_added(NaturalSplineCurve{});
            break;
          case CurveType::_Count:
            util::panic("AddSceneObjectPopup failed with object unexpected type");
        }
      }
    }
    ImGui::EndPopup();
  }

  static PatchSurfaceType chosen_patch_surface_type;
  static bool open_patch_surface_modal = false;
  if (ImGui::BeginPopup("AddPatchesPopup")) {
    for (const auto [type, name] : kPatchSurfaceNames) {
      if (ImGui::Selectable(name.c_str())) {
        chosen_patch_surface_type = type;
        open_patch_surface_modal  = true;
      }
    }

    ImGui::EndPopup();
  }
  if (open_patch_surface_modal) {
    ImGui::mini::OpenModal("Add Patch Surface");
    open_patch_surface_modal = false;
  }

  {
    static auto info = ImGui::mini::PatchSurfaceInfo{
        .r      = 3.F,
        .h      = 10.F,
        .size_x = 10.F,
        .size_y = 10.F,
    };
    if (ImGui::mini::AddPatchSurfaceModal("Add Patch Surface", info)) {
      switch (chosen_patch_surface_type) {
        case PatchSurfaceType::BezierPatchSurface:
          on_patch_surface_added(BezierPatches{}, info);
          break;
        case PatchSurfaceType::BPatchSurface:
          on_patch_surface_added(BPatches{}, info);
          break;
        case PatchSurfaceType::_Count:
          util::panic("AddSceneObjectPopup failed with object unexpected type");
      }
    }
  }

  static auto hide_points = false;
  ImGui::Checkbox("Hide points", &hide_points);
  ImGui::Separator();

  static std::optional<ObjectHandle> selected_single_handle  = std::nullopt;
  static constexpr zstring_view kPointDragAndDropPayloadType = "PointDragAndDropPayload";
  {
    auto drop_target = [&](const CurveHandle& h) {
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPointDragAndDropPayloadType.c_str())) {
          IM_ASSERT(payload->DataSize == sizeof(SceneObjectHandle));
          auto point_handle = *static_cast<SceneObjectHandle*>(payload->Data);
          m_.scene.push_back_point_to_curve(point_handle, h);
        }
        ImGui::EndDragDropTarget();
      }
    };

    auto drop_source = [&](const SceneObjectHandle& h) {
      if (auto opt = m_.scene.arena<SceneObject>().get_obj(h)) {
        const auto& obj = **opt;
        if (obj.has_type<Point>()) {
          if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(kPointDragAndDropPayloadType.c_str(), &h, sizeof(SceneObjectHandle));
            ImGui::Text("%s", obj.name.c_str());
            ImGui::EndDragDropSource();
          }
        }
      }
    };

    auto on_activate        = [&](const auto& h) { on_selection_add(h); };
    auto on_deactivate      = [&](const auto& h) { on_selection_remove(h); };
    auto on_activate_single = [&](const auto& h) { on_selection_set_single(h); };
    auto on_popup           = [&](const auto&) {
      selected_single_handle = get_single_handle_selection();
      ImGui::OpenPopup("SelectionPopup");
    };

    auto draw_item_scene_obj = [&](const SceneObjectHandle& h) {
      bool is_selected = m_.scene_obj_selection->contains(h);

      using T = ERAY_HANDLE_OBJ(h);
      ImGui::mini::ObjectListItem<T>(m_.scene, h, is_selected, on_activate, on_deactivate, on_activate_single,
                                     on_popup);
    };

    auto draw_item_non_scene_obj = [&](const auto& h) {
      bool is_selected = m_.non_scene_obj_selection->contains(h);

      using T = ERAY_HANDLE_OBJ(h);
      ImGui::mini::ObjectListItem<T>(m_.scene, h, is_selected, on_activate, on_deactivate, on_activate_single,
                                     on_popup);
    };

    auto skip_point = [&](const SceneObjectHandle& h) {
      if (hide_points) {
        if (auto opt = m_.scene.arena<SceneObject>().get_obj(h)) {
          const auto& obj = **opt;
          if (obj.has_type<Point>()) {
            return true;
          }
        }
      }

      return false;
    };

    for (const auto& handle : m_.scene.handles()) {
      if (std::visit(util::match{skip_point, [](const auto&) { return false; }}, handle)) {
        continue;
      }

      std::visit(util::match{draw_item_scene_obj, draw_item_non_scene_obj}, handle);
      std::visit(util::match{drop_target, [](const auto&) {}}, handle);
      std::visit(util::match{drop_source, [](const auto&) {}}, handle);
    }
  }

  static std::optional<ObjectHandle> rename_handle = std::nullopt;
  bool open_rename_modal                           = false;
  if (ImGui::BeginPopup("SelectionPopup")) {
    bool disabled = !m_.scene_obj_selection->is_points_only() || !m_.non_scene_obj_selection->is_empty();
    if (disabled) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Selectable(ICON_FA_LINK " Merge points")) {
      on_selection_merge();
    }

    if (ImGui::BeginMenu("Create Point List")) {
      for (const auto [type, name] : kCurveNames) {
        if (ImGui::Selectable(name.c_str())) {
          switch (type) {
            case CurveType::Polyline:
              on_curve_added_from_points_selection(Polyline{});
              break;
            case CurveType::MultisegmentBezierCurve:
              on_curve_added_from_points_selection(MultisegmentBezierCurve{});
              break;
            case CurveType::BSplineCurve:
              on_curve_added_from_points_selection(BSplineCurve{});
              break;
            case CurveType::NaturalSplineCurve:
              on_curve_added_from_points_selection(NaturalSplineCurve{});
              break;
            case CurveType::_Count:
              util::panic("AddCurvePopup failed with object unexpected type");
          }
        }
      }
      ImGui::EndMenu();
    }
    if (disabled) {
      ImGui::EndDisabled();
    }
    disabled = !selected_single_handle.has_value();
    if (disabled) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Selectable(ICON_FA_TEXT_WIDTH " Rename")) {
      if (selected_single_handle) {
        rename_handle     = selected_single_handle;
        open_rename_modal = true;
      }
    }

    if (ImGui::Selectable(ICON_FA_COPY " Copy")) {
      if (selected_single_handle) {
        std::visit(match{[this](const CObjectHandle auto& h) { m_.scene.clone_obj(h); }}, *selected_single_handle);
      }
    }
    if (disabled) {
      ImGui::EndDisabled();
    }

    if (ImGui::Selectable(ICON_FA_FILL " Fill in surface")) {
      on_fill_in_surface_from_selection();
    }
    ImGui::Separator();

    if (ImGui::Selectable(ICON_FA_TRASH " Delete")) {
      on_selection_deleted();
    }

    ImGui::EndPopup();
  }
  if (rename_handle) {
    if (open_rename_modal) {
      ImGui::mini::OpenModal("Rename object");
      open_rename_modal = false;
    }
    static std::string object_name;
    auto on_rename = [&](const CObjectHandle auto& h) {
      using T = ERAY_HANDLE_OBJ(h);
      if (auto opt = m_.scene.arena<T>().get_obj(h)) {
        auto& obj = **opt;

        object_name = obj.name;
        if (ImGui::mini::RenameModal("Rename object", object_name)) {
          obj.set_name(std::string(object_name));
          rename_handle = std::nullopt;
        }
      }
    };

    std::visit(match{on_rename}, *rename_handle);
  }

  ImGui::End();
}

void MiniCadApp::gui_transform_window() {
  ImGui::Begin(ICON_FA_UP_DOWN_LEFT_RIGHT " Transform");
  if (m_.scene_obj_selection->is_single_selection()) {
    if (auto opt = m_.scene.arena<SceneObject>().get_obj(m_.scene_obj_selection->first())) {
      auto& obj = **opt;

      std::visit(util::match{[&](auto& o) {
                   ImGui::Text("Type: %s", o.type_name().c_str());
                   ImGui::SameLine();
                   ImGui::Text("ID: %d", obj.id());
                 }},
                 obj.object);

      ImGui::mini::Transform(obj.transform, [&]() { obj.update(); });
    }
  } else if (m_.scene_obj_selection->is_multi_selection()) {
    ImGui::mini::Transform(m_.scene_obj_selection->transform,
                           [&]() { m_.scene_obj_selection->update_transforms(m_.scene, *m_.cursor); });
  }

  ImGui::End();
}

void MiniCadApp::gui_object_window() {
  static const zstring_view kReorderPointsDragAndDropPayloadType = "ReorderPointsDragAndDropPayload";
  auto reorder_dnd = ImGui::mini::ReorderDnD(kReorderPointsDragAndDropPayloadType);

  auto draw_curve = [&](const CurveHandle& h) {
    if (auto obj = m_.scene.arena<Curve>().get_obj(h)) {
      const auto& curve = *obj.value();
      auto type_name    = std::visit(util::match{[](auto& o) { return o.type_name(); }}, curve.object);

      ImGui::Text("Type: %s", type_name.c_str());
      ImGui::Text("Name: %s", curve.name.c_str());
      ImGui::SameLine();

      static std::string object_name;
      if (ImGui::Button("Rename")) {
        ImGui::mini::OpenModal("Rename object");
        object_name = curve.name;
      }
      if (ImGui::mini::RenameModal("Rename object", object_name)) {
        obj.value()->name = object_name;
      }

      if (!std::holds_alternative<Polyline>(curve.object)) {
        if (auto state = m_.scene.renderer().object_rs(curve.handle())) {
          if (auto* s = std::get_if<CurveRS>(&(*state))) {
            if (ImGui::Checkbox("Polyline", &s->show_polyline)) {
              m_.scene.renderer().push_object_rs_cmd(
                  CurveRSCommand(curve.handle(), CurveRSCommand::ShowPolyline(s->show_polyline)));
            }
          }
        }
      }

      if (ImGui::BeginTabBar("PointsTabBar")) {
        if (ImGui::BeginTabItem("Control Points")) {
          if (ImGui::Button("Add")) {
            on_point_created_in_point_list(h);
          }
          ImGui::SameLine();
          bool disable =
              !m_.scene_obj_selection->is_single_selection() || !curve.contains(m_.scene_obj_selection->first());
          if (disable) {
            ImGui::BeginDisabled();
          }
          if (ImGui::Button("Remove")) {
            m_.scene.remove_point_from_curve(m_.scene_obj_selection->first(), h);
          }
          if (disable) {
            ImGui::EndDisabled();
          }

          for (auto idx = 0U; const auto& p : curve.point_objects()) {
            ImGui::PushID(static_cast<int>(p.id()));
            bool is_selected = m_.scene_obj_selection->contains(p.handle());
            if (ImGui::Selectable(p.name.c_str(), is_selected)) {
              is_selected ? on_selection_remove(p.handle()) : on_selection_add(p.handle());
            }
            reorder_dnd.drag_and_drop_component(p, idx, true);
            ImGui::PopID();
            ++idx;
          }
          ImGui::EndTabItem();
        }

        if (std::holds_alternative<BSplineCurve>(curve.object) && ImGui::BeginTabItem("Bernstein Points")) {
          int id = 0;
          ImGui::Separator();
          for (const auto& group :
               std::get<BSplineCurve>(curve.object).bernstein_points() | std::views::slide(4) | std::views::stride(4)) {
            for (auto p : group) {
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
      on_points_reorder(h, reorder_dnd.source, reorder_dnd.before_dest, reorder_dnd.after_dest);
    }
  };

  auto draw_patch = [&](const PatchSurfaceHandle& h) {
    if (auto obj = m_.scene.arena<PatchSurface>().get_obj(h)) {
      const auto& patch = *obj.value();
      auto type_name    = std::visit(util::match{[](auto& o) { return o.type_name(); }}, patch.object);

      ImGui::Text("Type: %s", type_name.c_str());
      ImGui::Text("Name: %s", patch.name.c_str());
      ImGui::SameLine();

      static std::string object_name;
      if (ImGui::Button("Rename")) {
        ImGui::mini::OpenModal("Rename object");
        object_name = patch.name;
      }
      if (ImGui::mini::RenameModal("Rename object", object_name)) {
        obj.value()->name = object_name;
      }

      int t     = patch.tess_level();
      int st    = static_cast<int>(std::sqrt(t));
      int left  = static_cast<int>(std::pow(st - 1, 2));
      int right = static_cast<int>(std::pow(st + 1, 2));
      if (ImGui::InputInt("Tesselation level", &t)) {
        obj.value()->set_tess_level(t < patch.tess_level() ? left : right);
      }

      if (ImGui::Button(ICON_FA_DOWN_LONG " Insert row")) {
        obj.value()->insert_row_bottom();
      }
    }
  };

  auto draw_scene_object = [&](const SceneObjectHandle& h) {
    if (auto opt = m_.scene.arena<SceneObject>().get_obj(h)) {
      auto& obj = **opt;

      ImGui::Text("Name: %s", obj.name.c_str());
      ImGui::SameLine();
      static std::string object_name;
      if (ImGui::Button("Rename")) {
        ImGui::mini::OpenModal("Rename object");
        object_name = obj.name;
      }
      if (ImGui::mini::RenameModal("Rename object", object_name)) {
        obj.name = object_name;
      }

      std::visit(util::match{[&](auto& o) {
                   ImGui::Text("Type: %s", o.type_name().c_str());
                   ImGui::SameLine();
                   ImGui::Text("ID: %d", obj.id());
                 }},
                 obj.object);

      auto draw_torus = [&](Torus& t) {
        if (ImGui::SliderInt("Tess Level X", &t.tess_level.x, kMinTessLevel, kMaxTessLevel)) {
          obj.update();
        }
        if (ImGui::SliderInt("Tess Level Y", &t.tess_level.y, kMinTessLevel, kMaxTessLevel)) {
          obj.update();
        }
        if (ImGui::SliderFloat("Rad Minor", &t.minor_radius, 0.1F, t.major_radius)) {
          obj.update();
        }
        if (ImGui::SliderFloat("Rad Major", &t.major_radius, t.minor_radius, 5.F)) {
          obj.update();
        }
      };

      auto draw_point = [&](Point& /*p*/) {};

      std::visit(util::match{draw_point, draw_torus}, obj.object);
    }
  };

  auto draw_fill_in_surface = [&](const FillInSurfaceHandle& h) {
    if (auto obj = m_.scene.arena<FillInSurface>().get_obj(h)) {
      const auto& patch = *obj.value();
      auto type_name    = std::visit(util::match{[](auto& o) { return o.type_name(); }}, patch.object);

      ImGui::Text("Type: %s", type_name.c_str());
      ImGui::Text("Name: %s", patch.name.c_str());
      ImGui::SameLine();

      static std::string object_name;
      if (ImGui::Button("Rename")) {
        ImGui::mini::OpenModal("Rename object");
        object_name = patch.name;
      }
      if (ImGui::mini::RenameModal("Rename object", object_name)) {
        obj.value()->name = object_name;
      }

      int t     = patch.tess_level();
      int st    = static_cast<int>(std::sqrt(t));
      int left  = static_cast<int>(std::pow(st - 1, 2));
      int right = static_cast<int>(std::pow(st + 1, 2));
      if (ImGui::InputInt("Tesselation level", &t)) {
        obj.value()->set_tess_level(t < patch.tess_level() ? left : right);
      }
    }
  };

  ImGui::Begin(ICON_FA_PEN " Object");
  if (m_.non_scene_obj_selection->is_single_selection()) {  // non scene objects have priority
    std::visit(util::match{draw_curve, draw_patch, draw_fill_in_surface}, m_.non_scene_obj_selection->first());
  } else if (!m_.non_scene_obj_selection->is_single_selection() && m_.scene_obj_selection->is_single_selection()) {
    draw_scene_object(m_.scene_obj_selection->first());
  }
  ImGui::End();
}

void MiniCadApp::render_gui(Duration /* delta */) {
  ImGui::ShowDemoWindow();

  ImGui::Begin("MiNI CAD");
  {
    static const std::array<os::FileDialog::FilterItem, 1> kFileDialogFilters = {
        os::FileDialog::FilterItem("Project", "json")};

    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open...")) {
      if (auto result =
              System::file_dialog().open_file([this](const auto& p) { this->on_project_open(p); }, kFileDialogFilters);
          !result) {
        util::Logger::err("Failed to open file dialog");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save as...")) {
      if (auto result = System::file_dialog().save_file([this](const auto& p) { this->on_project_save_as(p); },
                                                        kFileDialogFilters);
          !result) {
        util::Logger::err("Failed to open file dialog");
      }
    }

    ImGui::Text("FPS: %d", fps_);
    ImGui::Checkbox(ICON_FA_TABLE " Grid", &m_.grid_on);

    bool points = m_.scene.renderer().are_points_shown();
    if (ImGui::Checkbox(ICON_FA_CIRCLE " Points", &points)) {
      m_.scene.renderer().show_points(points);
    }

    bool polylines = m_.scene.renderer().are_polylines_shown();
    if (ImGui::Checkbox(ICON_FA_DRAW_POLYGON " Polylines", &polylines)) {
      m_.scene.renderer().show_polylines(polylines);
    }
  }
  ImGui::End();

  ImGui::Begin(ICON_FA_CAMERA_RETRO " Camera");
  if (ImGui::Checkbox("Orthographic", &m_.use_ortho)) {
  }

  if (ImGui::Button("XZ")) {
    m_.orbiting_camera_operator.look_at_plane(*m_.camera, OrbitingCameraOperator::Plane::XZ);
  }
  ImGui::SameLine();
  if (ImGui::Button("-XZ")) {
    m_.orbiting_camera_operator.look_at_plane(*m_.camera, OrbitingCameraOperator::Plane::MXZ);
  }
  if (ImGui::Button("XY")) {
    m_.orbiting_camera_operator.look_at_plane(*m_.camera, OrbitingCameraOperator::Plane::XY);
  }
  ImGui::SameLine();
  if (ImGui::Button("-XY")) {
    m_.orbiting_camera_operator.look_at_plane(*m_.camera, OrbitingCameraOperator::Plane::MXY);
  }
  if (ImGui::Button("YZ")) {
    m_.orbiting_camera_operator.look_at_plane(*m_.camera, OrbitingCameraOperator::Plane::YZ);
  }
  ImGui::SameLine();
  if (ImGui::Button("-YZ")) {
    m_.orbiting_camera_operator.look_at_plane(*m_.camera, OrbitingCameraOperator::Plane::MYZ);
  }
  if (ImGui::Button("Look at cursor")) {
    m_.camera_gimbal->set_local_pos(m_.cursor->transform.pos());
  }
  if (ImGui::Button("Look at origin")) {
    m_.camera_gimbal->set_local_pos(m_.scene_obj_selection->centroid());
  }
  ImGui::End();

  ImGui::Begin("Rendering");
  {
    bool anaglyph = m_.scene.renderer().is_anaglyph_rendering_enabled();
    if (ImGui::Checkbox("Anaglyph enabled", &anaglyph)) {
      m_.scene.renderer().set_anaglyph_rendering_enabled(anaglyph);
    }
    auto dist = m_.camera->stereo_convergence_distance();
    if (ImGui::SliderFloat("Focal length", &dist, 0.1F, 20.F)) {
      m_.camera->set_stereo_convergence_distance(dist);
    }
    static auto coeffs = eray::math::Vec3f(0.34F, 0.25F, 1.F);
    ImGui::DragFloat3("Coefficients", coeffs.data, 0.01F, 0.F, 1.F);
    m_.scene.renderer().set_anaglyph_output_color_coeffs(coeffs);
  }
  ImGui::End();

  gui_objects_list_window();
  gui_transform_window();
  gui_object_window();

  ImGui::Begin(ICON_FA_TOOLBOX " Tools");
  bool selected = m_.tool_state == ToolState::Cursor;
  if (ImGui::Selectable(ICON_FA_ARROW_POINTER " Cursor", selected)) {
    on_cursor_state_set();
  }
  selected = m_.tool_state == ToolState::Select;
  if (ImGui::Selectable(ICON_FA_SQUARE " Select", selected)) {
    on_select_state_set();
  }

  ImGui::End();

  static auto operation = ImGui::mini::gizmo::Operation::Translation;
  static auto mode      = ImGui::mini::gizmo::Mode::World;
  ImGui::Begin(ICON_FA_HAMMER " Tool");
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
      bool cursor_as_origin = m_.scene_obj_selection->is_using_custom_origin();
      if (ImGui::Checkbox("Use cursor as origin", &cursor_as_origin)) {
        m_.scene_obj_selection->use_custom_origin(m_.scene, cursor_as_origin);
        if (cursor_as_origin) {
          m_.scene_obj_selection->set_custom_origin(m_.scene, m_.cursor->transform.pos());
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

    if (m_.scene_obj_selection->is_multi_selection() || m_.scene_obj_selection->is_using_custom_origin()) {
      if (ImGui::mini::gizmo::Transform(m_.scene_obj_selection->transform, *m_.camera, mode, operation,
                                        [&]() { m_.scene_obj_selection->update_transforms(m_.scene, *m_.cursor); })) {
        m_.select_tool.end_box_select();
      }
    } else if (m_.scene_obj_selection->is_single_selection()) {
      if (auto o = m_.scene.arena<SceneObject>().get_obj(m_.scene_obj_selection->first())) {
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
  ZoneScoped;

  m_.camera->set_orthographic(m_.use_ortho);

  m_.scene.renderer().update(m_.scene);

  m_.scene.renderer().show_grid(m_.grid_on);
  m_.scene.renderer().billboard("cursor").position   = m_.cursor->transform.pos();
  m_.scene.renderer().billboard("centroid").show     = m_.scene_obj_selection->is_multi_selection();
  m_.scene.renderer().billboard("centroid").position = m_.scene_obj_selection->centroid();
  m_.scene.renderer().render(*m_.camera);
}

void MiniCadApp::update(Duration delta) {
  auto deltaf = std::chrono::duration<float>(delta).count();

  if (m_.orbiting_camera_operator.update(*m_.camera, *m_.camera_gimbal, math::Vec2f(window_->mouse_pos()), deltaf)) {
    m_.cursor->mark_dirty();
  }

  m_.cursor->update(*m_.camera, math::Vec2f(window_->mouse_pos_ndc()));
}

bool MiniCadApp::on_scene_object_added(SceneObjectVariant variant) {
  auto obj_handle = m_.scene.create_obj<SceneObject>(std::move(variant));
  if (!obj_handle) {
    Logger::err("Could not add a new curve");
    return false;
  }

  if (auto o = m_.scene.arena<SceneObject>().get_obj(*obj_handle)) {
    o.value()->transform.set_local_pos(m_.cursor->transform.pos());
    o.value()->update();
    on_selection_clear();
    on_selection_add(*obj_handle);
    util::Logger::info("Created scene object \"{}\"", o.value()->name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_point_created_in_point_list(const CurveHandle& handle) {
  auto obj_handle = m_.scene.create_obj<SceneObject>(Point{});
  if (!obj_handle) {
    Logger::err("Could not add a new curve");
    return false;
  }

  if (auto o = m_.scene.arena<SceneObject>().get_obj(*obj_handle)) {
    m_.scene.push_back_point_to_curve(*obj_handle, handle);
    o.value()->transform.set_local_pos(m_.cursor->transform.pos());
    o.value()->update();
    on_selection_clear();
    on_selection_add(*obj_handle);
    util::Logger::info("Created scene object \"{}\"", o.value()->name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_obj_deleted(const ObjectHandle& handle) {
  auto obj_deleted = [&](const auto& h) {
    using T = ERAY_HANDLE_OBJ(h);
    if (auto opt = m_.scene.arena<T>().get_obj(h)) {
      const auto& obj = **opt;
      if (!obj.can_be_deleted()) {
        return false;
      }
    }

    m_.scene.delete_obj<T>(h);
    return true;
  };

  return std::visit(util::match{obj_deleted}, handle);
}

bool MiniCadApp::on_curve_added(CurveVariant variant) {
  if (auto handle = m_.scene.create_obj<Curve>(std::move(variant))) {
    m_.non_scene_obj_selection->add(*handle);
    if (auto o = m_.scene.arena<Curve>().get_obj(*handle)) {
      Logger::info("Created curve \"{}\"", o.value()->name);

      // Natural splines don't need the polyline by default
      if (o.value()->has_type<NaturalSplineCurve>()) {
        m_.scene.renderer().push_object_rs_cmd(
            CurveRSCommand(o.value()->handle(), CurveRSCommand::ShowPolyline(false)));
      }
    }

    return true;
  }

  return false;
}

bool MiniCadApp::on_curve_added_from_points_selection(CurveVariant variant) {
  if (auto handle = m_.scene.create_obj<Curve>(std::move(variant))) {
    m_.non_scene_obj_selection->add(*handle);
    if (auto o = m_.scene.arena<Curve>().get_obj(*handle)) {
      Logger::info("Created curve \"{}\"", o.value()->name);

      if (m_.scene_obj_selection->is_points_only()) {
        for (const auto& h : *m_.scene_obj_selection) {
          if (!o.value()->push_back(h)) {
            Logger::warn("Could not add scene object with id to curve\"{}\"", h.obj_id);
          }
        }
      }
    }

    return true;
  }

  return false;
}

bool MiniCadApp::on_selection_merge() {
  if (!m_.scene_obj_selection->is_points_only()) {
    return false;
  }

  if (!m_.scene.merge_points(m_.scene_obj_selection->begin(), m_.scene_obj_selection->end())) {
    Logger::warn("Could not merge points");
    return false;
  }
  m_.scene_obj_selection->clear(m_.scene);
  return true;
}

bool MiniCadApp::on_curve_deleted(const CurveHandle& handle) {
  if (auto o = m_.scene.arena<Curve>().get_obj(handle)) {
    auto name = o.value()->name;
    m_.scene.delete_obj(handle);
    Logger::info("Deleted curve \"{}\"", name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_fill_in_surface_added(FillInSurfaceVariant variant, const BezierHole3Finder::BezierHole& hole) {
  auto neighborhood = FillInSurface::SurfaceNeighborhood::create(
      FillInSurface::SurfaceNeighbor{
          .boundaries = hole[0].boundary_,
          .handle     = hole[0].patch_surface_handle_,
      },
      FillInSurface::SurfaceNeighbor{
          .boundaries = hole[1].boundary_,
          .handle     = hole[1].patch_surface_handle_,
      },
      FillInSurface::SurfaceNeighbor{
          .boundaries = hole[2].boundary_,
          .handle     = hole[2].patch_surface_handle_,
      });

  if (m_.scene.fill_in_surface_exists(neighborhood.get_triangle())) {
    return false;
  }

  if (auto opt = m_.scene.create_obj_and_get<FillInSurface>(std::move(variant))) {
    auto& obj = **opt;
    m_.non_scene_obj_selection->add(obj.handle());

    if (!obj.init(std::move(neighborhood))) {
      Logger::err("Failed to create fill in surface \"{}\"", obj.name);
      return false;
    }

    Logger::info("Created fill in surface \"{}\"", obj.name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_patch_surface_added(PatchSurfaceVariant variant, const ImGui::mini::PatchSurfaceInfo& info) {
  if (auto handle = m_.scene.create_obj<PatchSurface>(std::move(variant))) {
    m_.non_scene_obj_selection->add(*handle);
    if (auto o = m_.scene.arena<PatchSurface>().get_obj(*handle)) {
      Logger::info("Created patch surface \"{}\"", o.value()->name);
      if (!info.cylinder) {
        auto starter = PlanePatchSurfaceStarter{.size = math::Vec2f(info.size_x, info.size_y)};
        o.value()->init_from_starter(starter,
                                     eray::math::Vec2u(static_cast<uint32_t>(info.x), static_cast<uint32_t>(info.y)));
      } else {
        auto starter = CylinderPatchSurfaceStarter{
            .radius = info.r,
            .height = info.h,
        };
        o.value()->init_from_starter(starter,
                                     eray::math::Vec2u(static_cast<uint32_t>(info.x), static_cast<uint32_t>(info.y)));
      }
    }

    return true;
  }

  return false;
}

bool MiniCadApp::on_patch_surface_deleted(const PatchSurfaceHandle& handle) {
  if (auto o = m_.scene.arena<PatchSurface>().get_obj(handle)) {
    auto name = o.value()->name;
    m_.scene.delete_obj(handle);
    Logger::info("Deleted patch surface object \"{}\"", name);
    return true;
  }

  return false;
}

bool MiniCadApp::on_points_reorder(const CurveHandle& handle, const std::optional<size_t>& source,
                                   const std::optional<size_t>& before_dest, const std::optional<size_t>& after_dest) {
  if (auto obj = m_.scene.arena<Curve>().get_obj(handle)) {
    if (source && before_dest) {
      if (!obj.value()->move_before(*before_dest, *source)) {
        util::Logger::warn("Could not reorder the points in curve {}.", obj.value()->name);
      }
      obj.value()->update();
      return true;
    }

    if (source && after_dest) {
      if (!obj.value()->move_after(*after_dest, *source)) {
        util::Logger::warn("Could not reorder the points in curve {}.", obj.value()->name);
      }
      obj.value()->update();
      return true;
    }
  }

  return false;
}

bool MiniCadApp::on_selection_add(const ObjectHandle& handle) {
  auto scene_obj = [&](const SceneObjectHandle& h) {
    m_.scene_obj_selection->add(m_.scene, h);
    m_.scene.renderer().push_object_rs_cmd(
        SceneObjectRSCommand(h, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Selected)));
  };

  auto fill_in_surface_obj = [&](const FillInSurfaceHandle& h) { m_.non_scene_obj_selection->add(h); };

  auto point_list_obj = [&](const auto& h) {
    using T = ERAY_HANDLE_OBJ(h);
    if (auto o = m_.scene.arena<T>().get_obj(h)) {
      auto handles = o.value()->point_handles();
      on_selection_add_many(handles.begin(), handles.end());
    }

    m_.non_scene_obj_selection->add(h);
  };

  std::visit(util::match{scene_obj, fill_in_surface_obj, point_list_obj}, handle);

  return true;
}

bool MiniCadApp::on_selection_remove(const ObjectHandle& handle) {
  auto scene_obj = [&](const SceneObjectHandle& h) {
    m_.scene_obj_selection->remove(m_.scene, h);
    m_.scene.renderer().push_object_rs_cmd(
        SceneObjectRSCommand(h, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Visible)));
  };

  auto fill_in_surface_obj = [&](const FillInSurfaceHandle& h) { m_.non_scene_obj_selection->remove(h); };

  auto point_list_obj = [&](const auto& h) {
    using T = ERAY_HANDLE_OBJ(h);
    if (auto o = m_.scene.arena<T>().get_obj(h)) {
      auto handles = o.value()->point_handles();
      on_selection_remove_many(handles.begin(), handles.end());
    }

    m_.non_scene_obj_selection->remove(h);
  };

  std::visit(util::match{scene_obj, fill_in_surface_obj, point_list_obj}, handle);

  return true;
}

bool MiniCadApp::on_selection_set_single(const ObjectHandle& handle) {
  on_selection_clear();
  on_selection_add(handle);
  return true;
}

bool MiniCadApp::on_selection_clear() {
  for (const auto& handle : *m_.scene_obj_selection) {
    m_.scene.renderer().push_object_rs_cmd(
        SceneObjectRSCommand(handle, SceneObjectRSCommand::UpdateObjectVisibility(VisibilityState::Visible)));
  }
  m_.scene_obj_selection->clear(m_.scene);
  m_.non_scene_obj_selection->clear();
  m_.helper_point_selection.clear();
  return true;
}

bool MiniCadApp::on_selection_deleted() {
  auto result = false;
  for (const auto& handle : *m_.scene_obj_selection) {
    result = on_obj_deleted(handle) || result;
  }
  for (const auto& handle : *m_.non_scene_obj_selection) {
    std::visit(util::match{[&](const auto& h) { result = on_obj_deleted(h) || result; }}, handle);
  }
  on_selection_clear();
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

bool MiniCadApp::on_project_open(const std::filesystem::path& path) {
  util::Logger::info("Received file path: {}", path.string());
  auto deserializer = JsonDeserializer::create();
  if (auto file = std::ifstream(path)) {
    std::ostringstream ss;
    ss << file.rdbuf();
    auto json = ss.str();

    if (auto result = deserializer.deserialize(m_.scene, json); !result) {
      util::Logger::err("Could deserialize file with path {}.", path.string());
    } else {
      m_.proj_path = path;
      util::Logger::succ("Loaded project from file: {}", path.string());
    }

  } else {
    util::Logger::err("Could not open file {}. Input stream could not be opened.", path.string());
  }

  return true;
}

bool MiniCadApp::on_project_save_as(const std::filesystem::path& path) {
  util::Logger::info("Received file path: {}", path.string());
  auto serializer = JsonSerializer::create();
  auto str        = serializer.serialize(m_.scene);
  if (auto file = std::ofstream(path); file) {
    file << str;
    m_.proj_path = path;
    util::Logger::succ("Saved project to file: {}", path.string());
  } else {
    util::Logger::err("Could save to file {}. Output stream could not be opened.", path.string());
  }
  return true;
}

bool MiniCadApp::on_project_save() {
  if (m_.proj_path) {
    on_project_save_as(*m_.proj_path);
    return true;
  }

  util::Logger::warn("Could not save the project. No project path is known.");
  return false;
}

bool MiniCadApp::on_tool_action_end() {
  if (m_.tool_state == ToolState::Select && m_.select_tool.is_box_select_active()) {
    m_.select_tool.end_box_select();
    auto box = m_.select_tool.box(math::Vec2f(window_->mouse_pos()));

    on_selection_clear();
    auto sampling_result = m_.scene.renderer().sample_mouse_pick_box(
        m_.scene, static_cast<size_t>(box.pos.x), static_cast<size_t>(box.pos.y), static_cast<size_t>(box.size.x),
        static_cast<size_t>(box.size.y));

    if (!sampling_result) {
      return true;
    }

    std::visit(util::match{
                   [&](SampledHelperPoint& helper_point) {
                     m_.helper_point_selection.set_selection(
                         {.parent = helper_point.c_handle, .helper_point = helper_point.helper_point_idx});
                   },
                   [&](SampledSceneObjects& scene_objs) {
                     on_selection_add_many(scene_objs.handles.begin(), scene_objs.handles.end());
                   },
               },
               *sampling_result);

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
    return true;
  }
  if (ev.key_code() == os::KeyCode::LeftControl) {
    m_.ctrl_pressed = true;
    return true;
  }
  if (m_.ctrl_pressed) {
    if (ev.key_code() == os::KeyCode::S) {
      on_project_save();
      return true;
    }
  }

  return false;
}

bool MiniCadApp::on_key_released(const eray::os::KeyReleasedEvent& ev) {
  if (ev.key_code() == os::KeyCode::LeftControl) {
    m_.ctrl_pressed = false;
    return true;
  }

  return false;
}

bool MiniCadApp::on_fill_in_surface_from_selection() {
  std::vector<PatchSurfaceHandle> handles;
  auto bezier_surfaces_extractor = [&handles, this](const PatchSurfaceHandle& h) {
    if (auto opt = m_.scene.arena<PatchSurface>().get_obj(h)) {
      const auto& obj = **opt;
      if (obj.has_type<BezierPatches>()) {
        handles.push_back(h);
      }
    }
  };

  for (const auto& handle : *m_.non_scene_obj_selection) {
    std::visit(util::match{bezier_surfaces_extractor, [](const auto&) {}}, handle);
  }

  if (handles.size() < 0) {
    return false;
  }

  on_selection_clear();
  if (auto opt = BezierHole3Finder::find_holes(m_.scene, handles)) {
    const auto& holes = *opt;
    for (const auto& hole : holes) {
      on_fill_in_surface_added(GregoryPatches{}, hole);
    }
  }

  return false;
}

std::optional<ObjectHandle> MiniCadApp::get_single_handle_selection() const {
  if (m_.non_scene_obj_selection->is_single_selection() && m_.scene_obj_selection->is_empty()) {
    return std::visit(match{[](const CObjectHandle auto& h) -> ObjectHandle { return h; }},
                      m_.non_scene_obj_selection->first());
  }
  if (m_.non_scene_obj_selection->is_empty() && m_.scene_obj_selection->is_single_selection()) {
    return m_.scene_obj_selection->first();
  }
  return std::nullopt;
}

MiniCadApp::~MiniCadApp() {}

}  // namespace mini
