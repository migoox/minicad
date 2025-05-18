#pragma once
#include <cstdint>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <optional>
#include <type_traits>
#include <variant>

namespace mini {

using SceneObjectId = std::uint32_t;
using CurveId       = std::uint32_t;

template <typename T>
using ref = std::reference_wrapper<T>;

class Scene;

class SceneObject;
using SceneObjectHandle = eray::util::Handle<SceneObject>;

class Curve;
using CurveHandle = eray::util::Handle<Curve>;

class PatchSurface;
using PatchSurfaceHandle = eray::util::Handle<PatchSurface>;

using PointListObjectHandle = std::variant<CurveHandle, PatchSurfaceHandle>;
using ObjectHandle          = std::variant<SceneObjectHandle, CurveHandle, PatchSurfaceHandle>;

template <typename T>
concept CObjectHandle =
    std::is_same_v<T, SceneObjectHandle> || std::is_same_v<T, CurveHandle> || std::is_same_v<T, PatchSurfaceHandle>;

template <typename T>
using ObserverPtr = eray::util::ObserverPtr<T>;

template <typename T>
using OptionalObserverPtr = std::optional<eray::util::ObserverPtr<T>>;

}  // namespace mini
