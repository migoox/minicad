// AUTO-GENERATED WITH https://app.json_schema.io/
//
//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     JsonProject data = nlohmann::json::parse(jsonString);

// NOLINTBEGIN

#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <stdexcept>
#include <vector>

#ifndef NLOHMANN_OPT_HELPER
#define NLOHMANN_OPT_HELPER
namespace nlohmann {
template <typename T>
struct adl_serializer<std::shared_ptr<T>> {
  static void to_json(json& j, const std::shared_ptr<T>& opt) {
    if (!opt)
      j = nullptr;
    else
      j = *opt;
  }

  static std::shared_ptr<T> from_json(const json& j) {
    if (j.is_null())
      return std::make_shared<T>();
    else
      return std::make_shared<T>(j.get<T>());
  }
};
template <typename T>
struct adl_serializer<std::optional<T>> {
  static void to_json(json& j, const std::optional<T>& opt) {
    if (!opt)
      j = nullptr;
    else
      j = *opt;
  }

  static std::optional<T> from_json(const json& j) {
    if (j.is_null())
      return std::make_optional<T>();
    else
      return std::make_optional<T>(j.get<T>());
  }
};
}  // namespace nlohmann
#endif

namespace json_schema {
using nlohmann::json;

class ClassMemberConstraints {
 private:
  std::optional<int64_t> min_int_value;
  std::optional<int64_t> max_int_value;
  std::optional<double> min_double_value;
  std::optional<double> max_double_value;
  std::optional<size_t> min_length;
  std::optional<size_t> max_length;
  std::optional<std::string> pattern;

 public:
  ClassMemberConstraints(std::optional<int64_t> min_int_value, std::optional<int64_t> max_int_value,
                         std::optional<double> min_double_value, std::optional<double> max_double_value,
                         std::optional<size_t> min_length, std::optional<size_t> max_length,
                         std::optional<std::string> pattern)
      : min_int_value(min_int_value),
        max_int_value(max_int_value),
        min_double_value(min_double_value),
        max_double_value(max_double_value),
        min_length(min_length),
        max_length(max_length),
        pattern(pattern) {}
  ClassMemberConstraints()          = default;
  virtual ~ClassMemberConstraints() = default;

  void set_min_int_value(int64_t min_int_value) { this->min_int_value = min_int_value; }
  auto get_min_int_value() const { return min_int_value; }

  void set_max_int_value(int64_t max_int_value) { this->max_int_value = max_int_value; }
  auto get_max_int_value() const { return max_int_value; }

  void set_min_double_value(double min_double_value) { this->min_double_value = min_double_value; }
  auto get_min_double_value() const { return min_double_value; }

  void set_max_double_value(double max_double_value) { this->max_double_value = max_double_value; }
  auto get_max_double_value() const { return max_double_value; }

  void set_min_length(size_t min_length) { this->min_length = min_length; }
  auto get_min_length() const { return min_length; }

  void set_max_length(size_t max_length) { this->max_length = max_length; }
  auto get_max_length() const { return max_length; }

  void set_pattern(const std::string& pattern) { this->pattern = pattern; }
  auto get_pattern() const { return pattern; }
};

class ClassMemberConstraintException : public std::runtime_error {
 public:
  ClassMemberConstraintException(const std::string& msg) : std::runtime_error(msg) {}
};

class ValueTooLowException : public ClassMemberConstraintException {
 public:
  ValueTooLowException(const std::string& msg) : ClassMemberConstraintException(msg) {}
};

class ValueTooHighException : public ClassMemberConstraintException {
 public:
  ValueTooHighException(const std::string& msg) : ClassMemberConstraintException(msg) {}
};

class ValueTooShortException : public ClassMemberConstraintException {
 public:
  ValueTooShortException(const std::string& msg) : ClassMemberConstraintException(msg) {}
};

class ValueTooLongException : public ClassMemberConstraintException {
 public:
  ValueTooLongException(const std::string& msg) : ClassMemberConstraintException(msg) {}
};

class InvalidPatternException : public ClassMemberConstraintException {
 public:
  InvalidPatternException(const std::string& msg) : ClassMemberConstraintException(msg) {}
};

inline void CheckConstraint(const std::string& name, const ClassMemberConstraints& c, int64_t value) {
  if (c.get_min_int_value() != std::nullopt && value < *c.get_min_int_value()) {
    throw ValueTooLowException("Value too low for " + name + " (" + std::to_string(value) + "<" +
                               std::to_string(*c.get_min_int_value()) + ")");
  }

  if (c.get_max_int_value() != std::nullopt && value > *c.get_max_int_value()) {
    throw ValueTooHighException("Value too high for " + name + " (" + std::to_string(value) + ">" +
                                std::to_string(*c.get_max_int_value()) + ")");
  }
}

inline void CheckConstraint(const std::string& name, const ClassMemberConstraints& c, double value) {
  if (c.get_min_double_value() != std::nullopt && value < *c.get_min_double_value()) {
    throw ValueTooLowException("Value too low for " + name + " (" + std::to_string(value) + "<" +
                               std::to_string(*c.get_min_double_value()) + ")");
  }

  if (c.get_max_double_value() != std::nullopt && value > *c.get_max_double_value()) {
    throw ValueTooHighException("Value too high for " + name + " (" + std::to_string(value) + ">" +
                                std::to_string(*c.get_max_double_value()) + ")");
  }
}

inline void CheckConstraint(const std::string& name, const ClassMemberConstraints& c, const std::string& value) {
  if (c.get_min_length() != std::nullopt && value.length() < *c.get_min_length()) {
    throw ValueTooShortException("Value too short for " + name + " (" + std::to_string(value.length()) + "<" +
                                 std::to_string(*c.get_min_length()) + ")");
  }

  if (c.get_max_length() != std::nullopt && value.length() > *c.get_max_length()) {
    throw ValueTooLongException("Value too long for " + name + " (" + std::to_string(value.length()) + ">" +
                                std::to_string(*c.get_max_length()) + ")");
  }

  if (c.get_pattern() != std::nullopt) {
    std::smatch result;
    std::regex_search(value, result, std::regex(*c.get_pattern()));
    if (result.empty()) {
      throw InvalidPatternException("Value doesn't match pattern for " + name + " (" + value +
                                    " != " + *c.get_pattern() + ")");
    }
  }
}

#ifndef NLOHMANN_UNTYPED_json_schema_HELPER
#define NLOHMANN_UNTYPED_json_schema_HELPER
inline json get_untyped(const json& j, const char* property) {
  if (j.find(property) != j.end()) {
    return j.at(property).get<json>();
  }
  return json();
}

inline json get_untyped(const json& j, std::string property) { return get_untyped(j, property.data()); }
#endif

#ifndef NLOHMANN_OPTIONAL_json_schema_HELPER
#define NLOHMANN_OPTIONAL_json_schema_HELPER
template <typename T>
inline std::shared_ptr<T> get_heap_optional(const json& j, const char* property) {
  auto it = j.find(property);
  if (it != j.end() && !it->is_null()) {
    return j.at(property).get<std::shared_ptr<T>>();
  }
  return std::shared_ptr<T>();
}

template <typename T>
inline std::shared_ptr<T> get_heap_optional(const json& j, std::string property) {
  return get_heap_optional<T>(j, property.data());
}
template <typename T>
inline std::optional<T> get_stack_optional(const json& j, const char* property) {
  auto it = j.find(property);
  if (it != j.end() && !it->is_null()) {
    return j.at(property).get<std::optional<T>>();
  }
  return std::optional<T>();
}

template <typename T>
inline std::optional<T> get_stack_optional(const json& j, std::string property) {
  return get_stack_optional<T>(j, property.data());
}
#endif

class ControlPointElement {
 public:
  ControlPointElement()
      : id_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                      std::nullopt) {}
  virtual ~ControlPointElement() = default;

 private:
  int64_t id;
  ClassMemberConstraints id_constraint;

 public:
  const int64_t& get_id() const { return id; }
  int64_t& get_mutable_id() { return id; }
  void set_id(const int64_t& value) {
    CheckConstraint("id", id_constraint, value);
    this->id = value;
  }
};

enum class ObjectType : int {
  BEZIER_C0         = 0,
  BEZIER_C2         = 1,
  BEZIER_SURFACE_C0 = 2,
  BEZIER_SURFACE_C2 = 3,
  CHAIN             = 4,
  INTERPOLATED_C2   = 5,
  TORUS             = 6,
  _Count            = 7
};

class Float3 {
 public:
  Float3()          = default;
  virtual ~Float3() = default;

 private:
  double x;
  double y;
  double z;

 public:
  const double& get_x() const { return x; }
  double& get_mutable_x() { return x; }
  void set_x(const double& value) { this->x = value; }

  const double& get_y() const { return y; }
  double& get_mutable_y() { return y; }
  void set_y(const double& value) { this->y = value; }

  const double& get_z() const { return z; }
  double& get_mutable_z() { return z; }
  void set_z(const double& value) { this->z = value; }
};

class Quaternion {
 public:
  Quaternion()          = default;
  virtual ~Quaternion() = default;

 private:
  double w;
  double x;
  double y;
  double z;

 public:
  const double& get_w() const { return w; }
  double& get_mutable_w() { return w; }
  void set_w(const double& value) { this->w = value; }

  const double& get_x() const { return x; }
  double& get_mutable_x() { return x; }
  void set_x(const double& value) { this->x = value; }

  const double& get_y() const { return y; }
  double& get_mutable_y() { return y; }
  void set_y(const double& value) { this->y = value; }

  const double& get_z() const { return z; }
  double& get_mutable_z() { return z; }
  void set_z(const double& value) { this->z = value; }
};

class Uint2 {
 public:
  Uint2()
      : u_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt),
        v_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt) {
  }
  virtual ~Uint2() = default;

 private:
  int64_t u;
  ClassMemberConstraints u_constraint;
  int64_t v;
  ClassMemberConstraints v_constraint;

 public:
  const int64_t& get_u() const { return u; }
  int64_t& get_mutable_u() { return u; }
  void set_u(const int64_t& value) {
    CheckConstraint("u", u_constraint, value);
    this->u = value;
  }

  const int64_t& get_v() const { return v; }
  int64_t& get_mutable_v() { return v; }
  void set_v(const int64_t& value) {
    CheckConstraint("v", v_constraint, value);
    this->v = value;
  }
};

class Geometry {
 public:
  Geometry()
      : id_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt),
        large_radius_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt),
        small_radius_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                                std::nullopt) {}
  virtual ~Geometry() = default;

 private:
  int64_t id;
  ClassMemberConstraints id_constraint;
  std::optional<double> large_radius;
  ClassMemberConstraints large_radius_constraint;
  std::optional<std::string> name;
  ObjectType object_type;
  std::optional<Float3> position;
  std::optional<Quaternion> rotation;
  std::optional<Uint2> samples;
  std::optional<Float3> scale;
  std::optional<double> small_radius;
  ClassMemberConstraints small_radius_constraint;
  std::optional<std::vector<ControlPointElement>> control_points;
  std::optional<Uint2> size;

 public:
  const int64_t& get_id() const { return id; }
  int64_t& get_mutable_id() { return id; }
  void set_id(const int64_t& value) {
    CheckConstraint("id", id_constraint, value);
    this->id = value;
  }

  std::optional<double> get_large_radius() const { return large_radius; }
  void set_large_radius(std::optional<double> value) {
    if (value) CheckConstraint("large_radius", large_radius_constraint, *value);
    this->large_radius = value;
  }

  std::optional<std::string> get_name() const { return name; }
  void set_name(std::optional<std::string> value) { this->name = value; }

  const ObjectType& get_object_type() const { return object_type; }
  ObjectType& get_mutable_object_type() { return object_type; }
  void set_object_type(const ObjectType& value) { this->object_type = value; }

  std::optional<Float3> get_position() const { return position; }
  void set_position(std::optional<Float3> value) { this->position = value; }

  std::optional<Quaternion> get_rotation() const { return rotation; }
  void set_rotation(std::optional<Quaternion> value) { this->rotation = value; }

  std::optional<Uint2> get_samples() const { return samples; }
  void set_samples(std::optional<Uint2> value) { this->samples = value; }

  std::optional<Float3> get_scale() const { return scale; }
  void set_scale(std::optional<Float3> value) { this->scale = value; }

  std::optional<double> get_small_radius() const { return small_radius; }
  void set_small_radius(std::optional<double> value) {
    if (value) CheckConstraint("small_radius", small_radius_constraint, *value);
    this->small_radius = value;
  }

  std::optional<std::vector<ControlPointElement>> get_control_points() const { return control_points; }
  void set_control_points(std::optional<std::vector<ControlPointElement>> value) { this->control_points = value; }

  std::optional<Uint2> get_size() const { return size; }
  void set_size(std::optional<Uint2> value) { this->size = value; }
};

class PointElement {
 public:
  PointElement()
      : id_constraint(std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
                      std::nullopt) {}
  virtual ~PointElement() = default;

 private:
  int64_t id;
  ClassMemberConstraints id_constraint;
  std::optional<std::string> name;
  Float3 position;

 public:
  const int64_t& get_id() const { return id; }
  int64_t& get_mutable_id() { return id; }
  void set_id(const int64_t& value) {
    CheckConstraint("id", id_constraint, value);
    this->id = value;
  }

  std::optional<std::string> get_name() const { return name; }
  void set_name(std::optional<std::string> value) { this->name = value; }

  const Float3& get_position() const { return position; }
  Float3& get_mutable_position() { return position; }
  void set_position(const Float3& value) { this->position = value; }
};

class JsonProject {
 public:
  JsonProject()          = default;
  virtual ~JsonProject() = default;

 private:
  std::optional<std::vector<Geometry>> geometry;
  std::optional<std::vector<PointElement>> points;

 public:
  const std::optional<std::vector<Geometry>>& get_geometry() const { return geometry; }
  void set_geometry(std::optional<std::vector<Geometry>> value) { this->geometry = value; }
  void add_geometry(Geometry&& value) {
    if (!geometry) {
      geometry = std::vector<Geometry>();
    }

    this->geometry->emplace_back(std::move(value));
  }

  const std::optional<std::vector<PointElement>>& get_points() const { return points; }
  void set_points(std::optional<std::vector<PointElement>> value) { this->points = value; }
  void add_point(PointElement&& value) {
    if (!points) {
      points = std::vector<PointElement>();
    }

    this->points->emplace_back(std::move(value));
  }
};
}  // namespace json_schema

namespace json_schema {
void from_json(const json& j, ControlPointElement& x);
void to_json(json& j, const ControlPointElement& x);

void from_json(const json& j, Float3& x);
void to_json(json& j, const Float3& x);

void from_json(const json& j, Quaternion& x);
void to_json(json& j, const Quaternion& x);

void from_json(const json& j, Uint2& x);
void to_json(json& j, const Uint2& x);

void from_json(const json& j, Geometry& x);
void to_json(json& j, const Geometry& x);

void from_json(const json& j, PointElement& x);
void to_json(json& j, const PointElement& x);

void from_json(const json& j, JsonProject& x);
void to_json(json& j, const JsonProject& x);

void from_json(const json& j, ObjectType& x);
void to_json(json& j, const ObjectType& x);

inline void from_json(const json& j, ControlPointElement& x) { x.set_id(j.at("id").get<int64_t>()); }

inline void to_json(json& j, const ControlPointElement& x) {
  j       = json::object();
  j["id"] = x.get_id();
}

inline void from_json(const json& j, Float3& x) {
  x.set_x(j.at("x").get<double>());
  x.set_y(j.at("y").get<double>());
  x.set_z(j.at("z").get<double>());
}

inline void to_json(json& j, const Float3& x) {
  j      = json::object();
  j["x"] = x.get_x();
  j["y"] = x.get_y();
  j["z"] = x.get_z();
}

inline void from_json(const json& j, Quaternion& x) {
  x.set_w(j.at("w").get<double>());
  x.set_x(j.at("x").get<double>());
  x.set_y(j.at("y").get<double>());
  x.set_z(j.at("z").get<double>());
}

inline void to_json(json& j, const Quaternion& x) {
  j      = json::object();
  j["w"] = x.get_w();
  j["x"] = x.get_x();
  j["y"] = x.get_y();
  j["z"] = x.get_z();
}

inline void from_json(const json& j, Uint2& x) {
  x.set_u(j.at("u").get<int64_t>());
  x.set_v(j.at("v").get<int64_t>());
}

inline void to_json(json& j, const Uint2& x) {
  j      = json::object();
  j["u"] = x.get_u();
  j["v"] = x.get_v();
}

inline void from_json(const json& j, Geometry& x) {
  x.set_id(j.at("id").get<int64_t>());
  x.set_large_radius(get_stack_optional<double>(j, "largeRadius"));
  x.set_name(get_stack_optional<std::string>(j, "name"));
  x.set_object_type(j.at("objectType").get<ObjectType>());
  x.set_position(get_stack_optional<Float3>(j, "position"));
  x.set_rotation(get_stack_optional<Quaternion>(j, "rotation"));
  x.set_samples(get_stack_optional<Uint2>(j, "samples"));
  x.set_scale(get_stack_optional<Float3>(j, "scale"));
  x.set_small_radius(get_stack_optional<double>(j, "smallRadius"));
  x.set_control_points(get_stack_optional<std::vector<ControlPointElement>>(j, "controlPoints"));
  x.set_size(get_stack_optional<Uint2>(j, "size"));
}

inline void to_json(json& j, const Geometry& x) {
  j                  = json::object();
  j["id"]            = x.get_id();
  j["largeRadius"]   = x.get_large_radius();
  j["name"]          = x.get_name();
  j["objectType"]    = x.get_object_type();
  j["position"]      = x.get_position();
  j["rotation"]      = x.get_rotation();
  j["samples"]       = x.get_samples();
  j["scale"]         = x.get_scale();
  j["smallRadius"]   = x.get_small_radius();
  j["controlPoints"] = x.get_control_points();
  j["size"]          = x.get_size();
}

inline void from_json(const json& j, PointElement& x) {
  x.set_id(j.at("id").get<int64_t>());
  x.set_name(get_stack_optional<std::string>(j, "name"));
  x.set_position(j.at("position").get<Float3>());
}

inline void to_json(json& j, const PointElement& x) {
  j             = json::object();
  j["id"]       = x.get_id();
  j["name"]     = x.get_name();
  j["position"] = x.get_position();
}

inline void from_json(const json& j, JsonProject& x) {
  x.set_geometry(get_stack_optional<std::vector<Geometry>>(j, "geometry"));
  x.set_points(get_stack_optional<std::vector<PointElement>>(j, "points"));
}

inline void to_json(json& j, const JsonProject& x) {
  j             = json::object();
  j["geometry"] = x.get_geometry();
  j["points"]   = x.get_points();
}

inline void from_json(const json& j, ObjectType& x) {
  if (j == "bezierC0")
    x = ObjectType::BEZIER_C0;
  else if (j == "bezierC2")
    x = ObjectType::BEZIER_C2;
  else if (j == "bezierSurfaceC0")
    x = ObjectType::BEZIER_SURFACE_C0;
  else if (j == "bezierSurfaceC2")
    x = ObjectType::BEZIER_SURFACE_C2;
  else if (j == "chain")
    x = ObjectType::CHAIN;
  else if (j == "interpolatedC2")
    x = ObjectType::INTERPOLATED_C2;
  else if (j == "torus")
    x = ObjectType::TORUS;
  else {
    throw std::runtime_error("Input JSON does not conform to schema!");
  }
}

inline void to_json(json& j, const ObjectType& x) {
  switch (x) {
    case ObjectType::BEZIER_C0:
      j = "bezierC0";
      break;
    case ObjectType::BEZIER_C2:
      j = "bezierC2";
      break;
    case ObjectType::BEZIER_SURFACE_C0:
      j = "bezierSurfaceC0";
      break;
    case ObjectType::BEZIER_SURFACE_C2:
      j = "bezierSurfaceC2";
      break;
    case ObjectType::CHAIN:
      j = "chain";
      break;
    case ObjectType::INTERPOLATED_C2:
      j = "interpolatedC2";
      break;
    case ObjectType::TORUS:
      j = "torus";
      break;
    default:
      throw std::runtime_error("Unexpected value in enumeration \"ObjectType\": " +
                               std::to_string(static_cast<int>(x)));
  }
}
}  // namespace json_schema

// NOLINTEND
