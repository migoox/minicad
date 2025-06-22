#pragma once
#include <glad/gl.h>

#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/util/ruleof.hpp>
#include <vector>

namespace mini {
struct PointsVAO {
 public:
  PointsVAO() = delete;
  ERAY_DEFAULT_MOVE(PointsVAO)

  static PointsVAO create() {
    auto vbo_layout = eray::driver::gl::VertexBuffer::Layout();
    vbo_layout.add_attribute<float>("pos", 0, 3);
    return PointsVAO(
        eray::driver::gl::SimpleVertexArray::create(eray::driver::gl::VertexBuffer::create(std::move(vbo_layout))));
  }

  ~PointsVAO() = default;

  void add(const eray::math::Vec3f& p1, const eray::math::Vec3f& p2) {
    points_.push_back(p1.x);
    points_.push_back(p1.y);
    points_.push_back(p1.z);

    points_.push_back(p2.x);
    points_.push_back(p2.y);
    points_.push_back(p2.z);

    dirty_ = true;
  }

  void clear() {
    points_.clear();
    dirty_ = true;
  }

  void sync() {
    if (!dirty_) {
      return;
    }

    glNamedBufferData(vao_.vbo().handle().get(), static_cast<GLsizeiptr>(points_.size() * sizeof(float)),
                      points_.data(), GL_DYNAMIC_DRAW);
    dirty_ = false;
  }

  void bind() const { vao_.bind(); }

  const eray::driver::gl::VertexArrayHandle& vao_handle() { return vao_.handle(); }

  size_t vertex_count() const { return points_.size() / 3; }

 private:
  explicit PointsVAO(eray::driver::gl::SimpleVertexArray&& vao) : vao_(std::move(vao)) {}

 private:
  eray::driver::gl::SimpleVertexArray vao_;
  std::vector<float> points_;
  bool dirty_ = false;
};
}  // namespace mini
