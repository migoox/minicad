#pragma once
#include <cstdint>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/zstring_view.hpp>
#include <optional>
#include <type_traits>
#include <variant>

namespace mini {

using SceneObjectId = std::uint32_t;
using CurveId       = std::uint32_t;

template <typename T>
using ref = std::reference_wrapper<T>;

class Scene;

class PointObject;
using PointObjectHandle = eray::util::Handle<PointObject>;

class ParamPrimitive;
using ParamPrimitiveHandle = eray::util::Handle<ParamPrimitive>;

class Curve;
using CurveHandle = eray::util::Handle<Curve>;

class PatchSurface;
using PatchSurfaceHandle = eray::util::Handle<PatchSurface>;

class FillInSurface;
using FillInSurfaceHandle = eray::util::Handle<FillInSurface>;

class ApproxCurve;
using ApproxCurveHandle = eray::util::Handle<ApproxCurve>;

using PointListObjectHandle   = std::variant<CurveHandle, PatchSurfaceHandle>;
using ParametricSurfaceHandle = std::variant<PatchSurfaceHandle>;
using NonTransformableObjectHandle =
    std::variant<CurveHandle, PatchSurfaceHandle, FillInSurfaceHandle, ApproxCurveHandle>;
using TransformableObjectHandle = std::variant<ParamPrimitiveHandle, PointObjectHandle>;

using ObjectHandle = std::variant<PointObjectHandle, CurveHandle, PatchSurfaceHandle, FillInSurfaceHandle,
                                  ApproxCurveHandle, ParamPrimitiveHandle>;

template <typename T>
concept CObjectHandle = std::is_same_v<T, PointObjectHandle> || std::is_same_v<T, CurveHandle> ||
                        std::is_same_v<T, PatchSurfaceHandle> || std::is_same_v<T, FillInSurfaceHandle> ||
                        std::is_same_v<T, ApproxCurveHandle> || std::is_same_v<T, ParamPrimitiveHandle>;

template <typename T>
using ObserverPtr = eray::util::ObserverPtr<T>;

template <typename T>
using OptionalObserverPtr = std::optional<eray::util::ObserverPtr<T>>;

template <typename T>
concept CObjectVariant = requires {
  { T::type_name() } -> std::same_as<zstring_view>;
};

// NOLINTBEGIN
#define MINI_VALIDATE_VARIANT_TYPES(TVariant, CVariant)                      \
  template <typename Variant>                                                \
  struct ValidateVariant_##TVariant;                                         \
                                                                             \
  template <typename... Types>                                               \
  struct ValidateVariant_##TVariant<std::variant<Types...>> {                \
    static constexpr bool kAreVariantTypesValid = (CVariant<Types> && ...);  \
  };                                                                         \
                                                                             \
  static_assert(ValidateVariant_##TVariant<TVariant>::kAreVariantTypesValid, \
                "Not all variant types satisfy the required constraint")
// NOLINTEND

}  // namespace mini
