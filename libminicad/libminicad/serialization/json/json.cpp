#include "nlohmann/json.hpp"

#include <liberay/math/quat.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/variant_match.hpp>
#include <libminicad/scene/scene_object.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <libminicad/serialization/json/json.hpp>
#include <libminicad/serialization/json/schema.hpp>
#include <string>
#include <unordered_map>

namespace mini {

JsonSerializer JsonSerializer::create() {
  return JsonSerializer(Members{
      .id_map = std::unordered_map<SceneObjectHandle, std::int64_t>(),
  });
}

std::string JsonSerializer::serialize(const Scene& scene) {
  m_.id_map.clear();

  int64_t curr_id = 0;
  auto project    = json_schema::JsonProject();

  auto scene_objs = scene.arena<SceneObject>().objs();
  for (const auto& obj : scene_objs) {
    m_.id_map.emplace(obj.handle(), curr_id);

    auto p        = obj.transform.pos();
    auto target_p = json_schema::Float3();
    target_p.set_x(static_cast<double>(p.x));
    target_p.set_y(static_cast<double>(p.y));
    target_p.set_z(static_cast<double>(p.z));

    std::visit(eray::util::match{
                   [&](const Point&) {
                     auto point_element = json_schema::PointElement();
                     point_element.set_id(curr_id);
                     point_element.set_position(target_p);
                     point_element.set_name(obj.name);
                     project.add_point(std::move(point_element));
                   },  //
                   [&](const Torus& torus) {
                     auto geometry = json_schema::Geometry();
                     geometry.set_object_type(json_schema::ObjectType::TORUS);
                     geometry.set_id(curr_id);
                     geometry.set_name(obj.name);

                     geometry.set_large_radius(torus.major_radius);
                     geometry.set_small_radius(torus.minor_radius);

                     auto target_uv = json_schema::Uint2();
                     target_uv.set_u(torus.tess_level.x);
                     target_uv.set_v(torus.tess_level.y);
                     geometry.set_samples(target_uv);

                     geometry.set_position(target_p);

                     auto q        = obj.transform.rot();
                     auto target_q = json_schema::Quaternion();
                     target_q.set_w(static_cast<double>(q.w));
                     target_q.set_x(static_cast<double>(q.x));
                     target_q.set_y(static_cast<double>(q.y));
                     target_q.set_z(static_cast<double>(q.z));
                     geometry.set_rotation(target_q);

                     auto s        = obj.transform.scale();
                     auto target_s = json_schema::Float3();
                     target_s.set_x(static_cast<double>(s.x));
                     target_s.set_y(static_cast<double>(s.y));
                     target_s.set_z(static_cast<double>(s.z));
                     geometry.set_scale(target_s);

                     project.add_geometry(std::move(geometry));
                   }  //
               },
               obj.object);
    ++curr_id;
  }

  auto control_points = std::vector<json_schema::ControlPointElement>();
  auto curves         = scene.arena<Curve>().objs();
  for (const auto& curve : curves) {
    auto geometry = json_schema::Geometry();
    std::visit(
        eray::util::match{
            [&](const Polyline&) { geometry.set_object_type(json_schema::ObjectType::CHAIN); },
            [&](const MultisegmentBezierCurve&) { geometry.set_object_type(json_schema::ObjectType::BEZIER_C0); },
            [&](const BSplineCurve&) { geometry.set_object_type(json_schema::ObjectType::BEZIER_C2); },
            [&](const NaturalSplineCurve&) { geometry.set_object_type(json_schema::ObjectType::INTERPOLATED_C2); },
        },
        curve.object);
    geometry.set_id(curr_id);
    geometry.set_name(curve.name);

    auto cp = json_schema::ControlPointElement();
    for (const auto& h : curve.point_handles()) {
      cp.set_id(m_.id_map[h]);
      control_points.push_back(cp);
    }
    geometry.set_control_points(control_points);

    project.add_geometry(std::move(geometry));
    control_points.clear();
    ++curr_id;
  }

  auto surfaces = scene.arena<PatchSurface>().objs();
  for (const auto& surface : surfaces) {
    auto geometry = json_schema::Geometry();
    std::visit(eray::util::match{
                   [&](const BezierPatches&) { geometry.set_object_type(json_schema::ObjectType::BEZIER_SURFACE_C0); },
                   [&](const BPatches&) { geometry.set_object_type(json_schema::ObjectType::BEZIER_SURFACE_C2); },
               },
               surface.object);
    geometry.set_id(curr_id);
    geometry.set_name(surface.name);

    auto cp = json_schema::ControlPointElement();
    for (const auto& h : surface.point_handles()) {
      cp.set_id(m_.id_map[h]);
      control_points.push_back(cp);
    }
    geometry.set_control_points(control_points);

    if (auto tess_level = surface.tess_level(0, 0)) {
      auto target_uv = json_schema::Uint2();
      target_uv.set_u(*tess_level);
      target_uv.set_v(*tess_level);
      geometry.set_samples(target_uv);
    } else {
      auto target_uv = json_schema::Uint2();
      target_uv.set_u(4);
      target_uv.set_v(4);
      geometry.set_samples(target_uv);
    }

    auto target_size = json_schema::Uint2();
    auto dim         = surface.control_points_dim();
    target_size.set_u(dim.x);
    target_size.set_v(dim.y);
    geometry.set_size(target_size);

    project.add_geometry(std::move(geometry));
    control_points.clear();
    ++curr_id;
  }

  auto j = nlohmann::json();
  json_schema::to_json(j, project);
  return nlohmann::to_string(j);
}

JsonDeserializer JsonDeserializer::create() {
  return JsonDeserializer(Members{
      .id_map = std::unordered_map<std::int64_t, SceneObjectHandle>(),
  });
}
std::expected<void, JsonDeserializer::JsonDeserializationError> JsonDeserializer::deserialize(Scene& scene,
                                                                                              const std::string& json) {
  if (!nlohmann::json::accept(json)) {
    eray::util::Logger::err("Deserialization failed due to invalid JSON input.");
    return std::unexpected(InvalidJson);
  }
  auto j       = nlohmann::json::parse(json);
  auto project = json_schema::JsonProject();
  json_schema::from_json(j, project);

  scene.clear();

  if (auto point_elements = project.get_points()) {
    for (const auto& point_element : *point_elements) {
      if (auto opt = scene.create_obj_and_get<SceneObject>(Point{})) {
        auto& obj = **opt;

        auto id = point_element.get_id();
        m_.id_map.emplace(id, obj.handle());

        if (auto name = point_element.get_name()) {
          obj.set_name(std::move(*name));
        }

        const auto& p = point_element.get_position();
        obj.transform.set_local_pos(eray::math::Vec3f(static_cast<float>(p.get_x()), static_cast<float>(p.get_y()),
                                                      static_cast<float>(p.get_z())));
        obj.update();
      }
    }
  }

  static constexpr auto kObjectTypeToVariant =
      eray::util::EnumMapper<json_schema::ObjectType,
                             std::variant<SceneObjectVariant, CurveVariant, PatchSurfaceVariant>>({
          {json_schema::ObjectType::TORUS, Torus{}},
          {json_schema::ObjectType::CHAIN, Polyline{}},
          {json_schema::ObjectType::BEZIER_C0, MultisegmentBezierCurve{}},
          {json_schema::ObjectType::BEZIER_C2, BSplineCurve{}},
          {json_schema::ObjectType::INTERPOLATED_C2, NaturalSplineCurve{}},
          {json_schema::ObjectType::BEZIER_SURFACE_C0, BezierPatches{}},
          {json_schema::ObjectType::BEZIER_SURFACE_C2, BPatches{}},
      });

  if (auto geometry_elements = project.get_geometry()) {
    for (const auto& geom_element : *geometry_elements) {
      auto variant = kObjectTypeToVariant[geom_element.get_object_type()];
      auto visitor = Visitor(*this, scene);
      std::visit(eray::util::match{[&](auto&& inner) {
                   std::visit(eray::util::match{[&](auto&& v) { visitor(v, geom_element); }}, inner);
                 }},
                 variant);
    }
  }

  return {};
}
void JsonDeserializer::Visitor::operator()(SceneObjectVariant&& v, const json_schema::Geometry& elem) {
  std::visit(  //
      eray::util::match{
          [&](Torus&& t) {
            if (auto r = elem.get_large_radius()) {
              t.major_radius = static_cast<float>(*r);
            } else {
              eray::util::Logger::warn("Expected large radius defined for torus with id {}", elem.get_id());
            }
            if (auto r = elem.get_small_radius()) {
              t.minor_radius = static_cast<float>(*r);
            } else {
              eray::util::Logger::warn("Expected small radius defined for torus with id {}", elem.get_id());
            }
            if (auto uv = elem.get_samples()) {
              t.tess_level = eray::math::Vec2i(static_cast<int>(uv->get_u()), static_cast<int>(uv->get_v()));
            } else {
              eray::util::Logger::warn("Expected samples defined for torus with id {}", elem.get_id());
            }

            if (auto opt = scene.create_obj_and_get<SceneObject>(std::move(t))) {
              auto& obj = **opt;
              if (auto name = elem.get_name()) {
                obj.set_name(std::move(*name));
              }

              if (const auto& p = elem.get_position()) {
                obj.transform.set_local_pos(eray::math::Vec3f(
                    static_cast<float>(p->get_x()), static_cast<float>(p->get_y()), static_cast<float>(p->get_y())));
              } else {
                eray::util::Logger::warn("Expected position defined for torus with id {}", elem.get_id());
              }

              if (const auto& q = elem.get_rotation()) {
                obj.transform.set_local_rot(
                    eray::math::Quatf(static_cast<float>(q->get_w()), static_cast<float>(q->get_x()),
                                      static_cast<float>(q->get_y()), static_cast<float>(q->get_z())));
              } else {
                eray::util::Logger::warn("Expected rotation defined for torus with id {}", elem.get_id());
              }

              if (const auto& s = elem.get_scale()) {
                obj.transform.set_local_scale(eray::math::Vec3f(
                    static_cast<float>(s->get_x()), static_cast<float>(s->get_y()), static_cast<float>(s->get_z())));
              } else {
                eray::util::Logger::warn("Expected rotation defined for torus with id {}", elem.get_id());
              }

              obj.update();
            }
          },
          [&](auto&&) {}},
      std::move(v));
}

void JsonDeserializer::Visitor::operator()(CurveVariant&& v, const json_schema::Geometry& elem) {
  auto opt = std::visit(
      eray::util::match{[&](auto&& v) { return scene.create_obj_and_get<Curve>(std::forward<decltype(v)>(v)); }},
      std::move(v));

  if (opt) {
    auto& obj = **opt;
    if (auto name = elem.get_name()) {
      obj.set_name(std::move(*name));
    }

    if (auto control_points = elem.get_control_points()) {
      for (auto& cp : *control_points) {
        const auto& h = deserializer.m_.id_map.at(cp.get_id());
        if (!obj.add(h)) {
          eray::util::Logger::err("Invalid point handle for curve with id {}", elem.get_id());
        }
      }
    }
    obj.update();
  }
}

void JsonDeserializer::Visitor::operator()(PatchSurfaceVariant&& v, const json_schema::Geometry& elem) {
  auto opt = std::visit(
      eray::util::match{[&](auto&& v) { return scene.create_obj_and_get<PatchSurface>(std::forward<decltype(v)>(v)); }},
      std::move(v));

  if (opt) {
    auto& obj = **opt;
    if (auto name = elem.get_name()) {
      obj.set_name(std::move(*name));
    }

    auto point_handles = std::vector<SceneObjectHandle>();
    if (auto control_points = elem.get_control_points()) {
      for (auto& cp : *control_points) {
        point_handles.push_back(deserializer.m_.id_map.at(cp.get_id()));
      }
    }

    if (auto s = elem.get_size()) {
      if (s->get_u() % 3 == 1 && s->get_v() % 3 == 1) {
        auto starter = PatchSurfaceStarter{PlanePatchSurfaceStarter{}};
        if (!obj.set_starter_from_points(
                starter,
                eray::math::Vec2u(static_cast<uint32_t>(s->get_u() / 3), static_cast<uint32_t>(s->get_v() / 3)),
                point_handles)) {
          eray::util::Logger::err("Could not add points to surface");
        }
      } else if (s->get_u() % 3 == 0 && s->get_v() % 3 == 1) {
        auto starter = PatchSurfaceStarter{CylinderPatchSurfaceStarter{}};
        if (!obj.set_starter_from_points(
                starter,
                eray::math::Vec2u(static_cast<uint32_t>(s->get_u() / 3), static_cast<uint32_t>(s->get_v() / 3)),
                point_handles)) {
          eray::util::Logger::err("Could not add points to surface");
        }
      } else if (s->get_u() % 3 == 1 && s->get_v() % 3 == 0) {
        auto starter = PatchSurfaceStarter{CylinderPatchSurfaceStarter{}};

        // transpose
        auto size_v = static_cast<size_t>(s->get_v());
        auto size_u = static_cast<size_t>(s->get_u());
        for (auto i = 0U; i < size_v; ++i) {
          for (auto j = i + 1; j < size_u; ++j) {
            std::swap(point_handles[i * size_u + j], point_handles[j * size_u + i]);
          }
        }

        if (!obj.set_starter_from_points(
                starter,
                eray::math::Vec2u(static_cast<uint32_t>(s->get_u() / 3), static_cast<uint32_t>(s->get_v() / 3)),
                point_handles)) {
          eray::util::Logger::err("Could not add points to surface");
        }
      } else {
        eray::util::Logger::err("Invalid surface size");
      }
    } else {
      eray::util::Logger::warn("Expected size for surface with id {}", elem.get_id());
    }
    if (auto s = elem.get_samples()) {
      if (s->get_u() != s->get_v()) {
        eray::util::Logger::warn("Expected the same u and v samples for surface with id {}", elem.get_id());
      }
      obj.set_tess_level_for_all(static_cast<int>(std::max(s->get_u(), s->get_v())));
    } else {
      eray::util::Logger::warn("Expected samples for surface with id {}", elem.get_id());
    }
    obj.update();
  }
}

}  // namespace mini
