#pragma once
#include <glad/gl.h>

#include <liberay/math/vec.hpp>
#include <vector>

namespace mini {
struct LineBuffer {
 public:
  LineBuffer() {
    glCreateVertexArrays(1, &vao_);
    glCreateBuffers(1, &vbo_);

    glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(float) * 3);

    glEnableVertexArrayAttrib(vao_, 0);
    glVertexArrayAttribFormat(vao_, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao_, 0, 0);
  }

  ~LineBuffer() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
  }

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

    glNamedBufferData(vbo_, static_cast<GLsizeiptr>(points_.size() * sizeof(float)), points_.data(), GL_DYNAMIC_DRAW);
    dirty_ = false;
  }

  void bind() const { glBindVertexArray(vao_); }

  size_t line_count() const { return points_.size() / 6; }
  size_t vertex_count() const { return points_.size() / 3; }

 private:
  GLuint vao_ = 0, vbo_ = 0;
  std::vector<float> points_;
  bool dirty_ = false;
};
}  // namespace mini
