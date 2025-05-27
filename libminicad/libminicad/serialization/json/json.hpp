#pragma once

#include <cstdint>
#include <libminicad/scene/scene.hpp>
#include <libminicad/scene/scene_object_handle.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>

#include "libminicad/serialization/json/schema.hpp"

namespace mini {

class JsonSerializer {
 public:
  JsonSerializer() = delete;
  static JsonSerializer create();

  /**
   * @brief Returns string containing serialized scene in json format.
   *
   * @return std::string
   */
  std::string serialize(const Scene& scene);

 private:
  struct Members {
    std::unordered_map<SceneObjectHandle, std::int64_t> id_map;
  } m_;

  explicit JsonSerializer(Members&& m) : m_(std::move(m)) {}
};

class JsonDeserializer {
 public:
  JsonDeserializer() = delete;
  static JsonDeserializer create();

  enum JsonDeserializationError : uint8_t {
    InvalidJson = 0,
  };

  /**
   * @brief Deserializes json string to provided scene.
   *
   * @return std::string
   */
  std::expected<void, JsonDeserializationError> deserialize(Scene& scene, const std::string& json);

 private:
  struct Members {
    std::unordered_map<std::int64_t, SceneObjectHandle> id_map;
  } m_;

  struct Visitor {
    explicit Visitor(JsonDeserializer& _deserializer, Scene& _scene) : deserializer(_deserializer), scene(_scene) {}
    void operator()(SceneObjectVariant&&, const json_schema::Geometry& elem);
    void operator()(CurveVariant&&, const json_schema::Geometry& elem);
    void operator()(PatchSurfaceVariant&&, const json_schema::Geometry& elem);

    JsonDeserializer& deserializer;  // NOLINT
    Scene& scene;                    // NOLINT
  };

  friend Visitor;

  explicit JsonDeserializer(Members&& m) : m_(std::move(m)) {}
};

}  // namespace mini
