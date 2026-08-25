/*
 * Copyright 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#ifdef __cplusplus

#include <algorithm>
#include <optional>
#include <type_traits>
#include <vector>

#include <audio_utils/template_utils.h>
#include <android/binder_enums.h>

namespace android::audio_utils {

using android::audio_utils::has_tag_and_get_tag_v;
using android::audio_utils::is_specialization_v;
using android::audio_utils::op_aggregate;

/**
 * Type of elements needs custom comparison for the elementwise ops.
 * When `CustomOpElementTypes` evaluated to true, custom comparison is implemented in this header.
 * When `CustomOpElementTypes` evaluated to false, fallback to std implementation.
 */
template <typename T>
concept CustomOpElementTypes =
    (std::is_class_v<T> && std::is_aggregate_v<T>) ||
    is_specialization_v<T, std::vector> || has_tag_and_get_tag_v<T>;

/**
 * Find the underlying value of AIDL union objects, and run an `op` with the underlying values
 */
template <typename Op, typename T, std::size_t... Is, typename... Ts>
void aidl_union_op_helper(Op&& op, std::index_sequence<Is...>, const T& first, const Ts&... rest) {
  (([&]() -> bool {
     const typename T::Tag TAG = static_cast<typename T::Tag>(Is);
     if (((first.getTag() == TAG) && ... && (rest.getTag() == TAG))) {
       // handle the case of a sub union class inside another union
       using FieldType = decltype(first.template get<TAG>());
       if constexpr (has_tag_and_get_tag_v<FieldType>) {
         static constexpr std::size_t tagSize = std::ranges::distance(
             ndk::enum_range<typename FieldType::Tag>().begin(),
             ndk::enum_range<typename FieldType::Tag>().end());
         return aidl_union_op_helper(op, std::make_index_sequence<tagSize>{},
                                     first.template get<TAG>(),
                                     rest.template get<TAG>()...);
       } else {
         op.template operator()<TAG>(first.template get<TAG>(), rest.template get<TAG>()...);
         // exit the index sequence
         return true;
       }
     } else {
       return false;
     }
   }()) ||
   ...);
}

// check if the class `T` is an AIDL union with `has_tag_and_get_tag_v`
template <typename Op, typename T, typename... Ts>
  requires(has_tag_and_get_tag_v<T> && ... && has_tag_and_get_tag_v<Ts>)
void aidl_union_op(Op&& op, const T& first, const Ts&... rest) {
  static constexpr std::size_t tagSize =
      std::ranges::distance(ndk::enum_range<typename T::Tag>().begin(),
                            ndk::enum_range<typename T::Tag>().end());
  aidl_union_op_helper(op, std::make_index_sequence<tagSize>{}, first, rest...);
}

/**
 * Utility functions for clamping values of different types within a specified
 * range of [min, max]. Supported types are evaluated with
 * `CustomOpElementTypes`.
 *
 * - For **structures**, each member is clamped individually and reassembled
 *   after clamping.
 * - For **vectors**, the `min` and `max` ranges (if defined) may have either
 *   one element or match the size of the target vector. If `min`/`max` have
 *   only one element, each target vector element is clamped within that range.
 *   If `min`/`max` match the target's size, each target element is clamped
 *   within the corresponding `min`/`max` elements.
 * - For **AIDL union** class, `aidl_union_op` is used to find the underlying
 *   value automatically first, and then do `elementwise_clamp` on the
 *   underlying value.
 * - For all other types, `std::clamp` is used directly, std::string
 *   comparison and clamp is performed lexicographically.
 *
 * The maximum number of members supported in a structure is `kMaxStructMember`
 * as defined in the template_utils.h header.
 */

/**
 * @brief Clamp function for aggregate types (structs).
 */
template <typename T>
  requires std::is_class_v<T> && std::is_aggregate_v<T>
[[nodiscard]]
std::optional<T> elementwise_clamp(const T& target, const T& min, const T& max);

template <typename T>
  requires has_tag_and_get_tag_v<T>
[[nodiscard]]
std::optional<T> elementwise_clamp(const T& target, const T& min, const T& max);

/**
 * @brief Clamp function for all other types, `std::clamp` is used.
 */
template <typename T>
  requires(!CustomOpElementTypes<T>)
[[nodiscard]]
std::optional<T> elementwise_clamp(const T& target, const T& min, const T& max) {
  if (min > max) {
    return std::nullopt;
  }
  return std::clamp(target, min, max);
}

/**
 * @brief Clamp function for vectors.
 *
 * Clamping each vector element within a specified range. The `min` and `max`
 * vectors may have either one element or the same number of elements as the
 * `target` vector.
 *
 * - If `min` or `max` contain only one element, each element in `target` is
 *   clamped by this single value.
 * - If `min` or `max` match `target` in size, each element in `target` is
 *   clamped by the corresponding elements in `min` and `max`.
 * - If size of `min` or `max` vector is neither 1 nor same size as `target`,
 *   the range will be considered as invalid, and `std::nullopt` will be
 *   returned.
 *
 * Some examples:
 * std::vector<int> target({3, 0, 5, 2});
 * std::vector<int> min({1});
 * std::vector<int> max({3});
 * elementwise_clamp(target, min, max) result will be std::vector({3, 1, 3, 2})
 *
 * std::vector<int> target({3, 0, 5, 2});
 * std::vector<int> min({1, 2, 3, 4});
 * std::vector<int> max({3, 4, 5, 6});
 * elementwise_clamp(target, min, max) result will be std::vector({3, 2, 5, 4})
 *
 * std::vector<int> target({3, 0, 5, 2});
 * std::vector<int> min({});
 * std::vector<int> max({3, 4});
 * elementwise_clamp(target, min, max) result will be std::nullopt
 */
template <typename T>
  requires is_specialization_v<T, std::vector>
[[nodiscard]]
std::optional<T> elementwise_clamp(const T& target, const T& min, const T& max) {
  using ElemType = typename T::value_type;

  const size_t min_size = min.size(), max_size = max.size(),
               target_size = target.size();
  if (min_size == 0 || max_size == 0 || target_size == 0) {
    return std::nullopt;
  }

  T result;
  result.reserve(target_size);

  if (min_size == 1 && max_size == 1) {
    const ElemType clamp_min = min[0], clamp_max = max[0];
    for (size_t i = 0; i < target_size; ++i) {
      auto clamped_elem = elementwise_clamp(target[i], clamp_min, clamp_max);
      if (clamped_elem) {
        result.emplace_back(*clamped_elem);
      } else {
        return std::nullopt;
      }
    }
  } else if (min_size == target_size && max_size == target_size) {
    for (size_t i = 0; i < target_size; ++i) {
      auto clamped_elem = elementwise_clamp(target[i], min[i], max[i]);
      if (clamped_elem) {
        result.emplace_back(*clamped_elem);
      } else {
        return std::nullopt;
      }
    }
  } else if (min_size == 1 && max_size == target_size) {
    const ElemType clamp_min = min[0];
    for (size_t i = 0; i < target_size; ++i) {
      auto clamped_elem = elementwise_clamp(target[i], clamp_min, max[i]);
      if (clamped_elem) {
        result.emplace_back(*clamped_elem);
      } else {
        return std::nullopt;
      }
    }
  } else if (min_size == target_size && max_size == 1) {
    const ElemType clamp_max = max[0];
    for (size_t i = 0; i < target_size; ++i) {
      auto clamped_elem = elementwise_clamp(target[i], min[i], clamp_max);
      if (clamped_elem) {
        result.emplace_back(*clamped_elem);
      } else {
        return std::nullopt;
      }
    }
  } else {
    // incompatible size
    return std::nullopt;
  }

  return result;
}

/**
 * @brief Clamp function for class and aggregate type (structs).
 *
 * Uses `opAggregate` with elementwise_clamp_op to perform clamping on each member of the
 * aggregate type `T`, the max number of supported members in `T` is
 * `kMaxStructMember`.
 */
template <typename T>
  requires std::is_class_v<T> && std::is_aggregate_v<T>
[[nodiscard]]
std::optional<T> elementwise_clamp(const T& target, const T& min, const T& max) {
  const auto elementwise_clamp_op = [](const auto& a, const auto& b, const auto& c) {
    return elementwise_clamp(a, b, c);
  };
  return op_aggregate(elementwise_clamp_op, target, min, max);
}

template <typename T>
  requires has_tag_and_get_tag_v<T>
[[nodiscard]]
std::optional<T> elementwise_clamp(const T& target, const T& min, const T& max) {
  std::optional<T> ret = std::nullopt;

  auto elementwise_clamp_op = [&]<typename T::Tag TAG, typename P>(
                          const P& p1, const P& p2, const P& p3) {
    auto p = elementwise_clamp(p1, p2, p3);
    if (!p) return;
    ret = T::template make<TAG>(*p);
  };

  aidl_union_op(elementwise_clamp_op, target, min, max);
  return ret;
}

/**
 * Utility functions to determine the element-wise min/max of two values with
 * same type. The `elementwise_min` function accepts two inputs and return
 * the element-wise min of them, while the `elementwise_max` function
 * calculates the element-wise max.
 *
 * - For **vectors**, the two input vectors may have either `0`, `1`, or `n`
 *   elements. If both input vectors have more than one element, their sizes
 *   must match. If either input vector has only one element, it is compared
 *   with each element of the other input vector.
 * - For **structures (aggregate types)**, each element field is compared
 *   individually, and the final result is reassembled from the element field
 *   comparison result.
 * - For **AIDL union** class, `aidl_union_op` is used to find the underlying
 *   value automatically first, and then do elementwise min/max on the
 *   underlying value.
 * - For all other types, `std::min`/`std::max` is used directly, std::string
 *   comparison and clamp is performed lexicographically.
 *
 * The maximum number of element fields supported in a structure is defined by
 * `android::audio_utils::kMaxStructMember` as defined in the `template_utils.h`
 * header.
 */

template <typename T>
  requires std::is_class_v<T> && std::is_aggregate_v<T>
[[nodiscard]]
std::optional<T> elementwise_min(const T& a, const T& b);

template <typename T>
  requires has_tag_and_get_tag_v<T>
[[nodiscard]]
std::optional<T> elementwise_min(const T& a, const T& b);

template <typename T>
  requires std::is_class_v<T> && std::is_aggregate_v<T>
[[nodiscard]]
std::optional<T> elementwise_max(const T& a, const T& b);

template <typename T>
  requires has_tag_and_get_tag_v<T>
[[nodiscard]]
std::optional<T> elementwise_max(const T& a, const T& b);

/**
 * @brief Determines the min/max for all other type values.
 *
 * @tparam T The target type.
 * @param a The first value.
 * @param b The second value.
 * @return The min/max of the two inputs.
 *
 * Example:
 * int a = 3;
 * int b = 5;
 * auto result = elementwise_min(a, b);  // result will be 3
 * auto result = elementwise_max(a, b);  // result will be 5
 */
template <typename T>
  requires(!CustomOpElementTypes<T>)
[[nodiscard]]
std::optional<T> elementwise_min(const T& a, const T& b) {
  return std::min(a, b);
}

template <typename T>
  requires(!CustomOpElementTypes<T>)
[[nodiscard]]
std::optional<T> elementwise_max(const T& a, const T& b) {
  return std::max(a, b);
}

/**
 * @brief Determines the element-wise min/max of two vectors by comparing
 * each corresponding element.
 *
 * This function calculates the element-wise min/max of two input vectors. The
 * valid sizes for input vectors `a` and `b` can be 0, 1, or `n` (where `n >
 * 1`). If both `a` and `b` contain more than one element, their sizes must be
 * equal. If either vector has only one element, that value will be compared
 * with each element of the other vector.
 *
 * Some examples:
 * std::vector<int> a({1, 2, 3, 4});
 * std::vector<int> b({3, 4, 5, 0});
 * elementwise_min(a, b) result will be std::vector({1, 2, 3, 0})
 * elementwise_max(a, b) result will be std::vector({3, 4, 5, 4})
 *
 * std::vector<int> a({1});
 * std::vector<int> b({3, 4, 5, 0});
 * elementwise_min(a, b) result will be std::vector({1, 1, 1, 0})
 * elementwise_max(a, b) result will be std::vector({3, 4, 5, 1})
 *
 * std::vector<int> a({1, 2, 3});
 * std::vector<int> b({});
 * elementwise_min(a, b) result will be std::vector({})
 * elementwise_max(a, b) result will be std::vector({1, 2, 3})
 *
 * std::vector<int> a({1, 2, 3, 4});
 * std::vector<int> b({3, 4, 0});
 * elementwise_min(a, b) and elementwise_max(a, b) result will be std::nullopt
 *
 * @tparam T The vector type.
 * @param a The first vector.
 * @param b The second vector.
 * @return A vector representing the element-wise min/max, or `std::nullopt` if
 * sizes are incompatible.
 */
template <typename T>
  requires is_specialization_v<T, std::vector>
[[nodiscard]]
std::optional<T> elementwise_min(const T& a, const T& b) {
  T result;
  const size_t a_size = a.size(), b_size = b.size();
  if (a_size == 0 || b_size == 0) {
    return result;
  }

  if (a_size == b_size) {
    for (size_t i = 0; i < a_size; ++i) {
      auto lower_elem = elementwise_min(a[i], b[i]);
      if (lower_elem) {
        result.emplace_back(*lower_elem);
      }
    }
  } else if (a_size == 1) {
    for (size_t i = 0; i < b_size; ++i) {
      auto lower_elem = elementwise_min(a[0], b[i]);
      if (lower_elem) {
        result.emplace_back(*lower_elem);
      }
    }
  } else if (b_size == 1) {
    for (size_t i = 0; i < a_size; ++i) {
      auto lower_elem = elementwise_min(a[i], b[0]);
      if (lower_elem) {
        result.emplace_back(*lower_elem);
      }
    }
  } else {
    // incompatible size
    return std::nullopt;
  }

  return result;
}

template <typename T>
  requires is_specialization_v<T, std::vector>
[[nodiscard]]
std::optional<T> elementwise_max(const T& a, const T& b) {
  T result;
  const size_t a_size = a.size(), b_size = b.size();
  if (a_size == 0) {
    result = b;
  } else if (b_size == 0) {
    result = a;
  } else if (a_size == b_size) {
    for (size_t i = 0; i < a_size; ++i) {
      auto upper_elem = elementwise_max(a[i], b[i]);
      if (upper_elem) result.emplace_back(*upper_elem);
    }
  } else if (a_size == 1) {
    for (size_t i = 0; i < b_size; ++i) {
      auto upper_elem = elementwise_max(a[0], b[i]);
      if (upper_elem) result.emplace_back(*upper_elem);
    }
  } else if (b_size == 1) {
    for (size_t i = 0; i < a_size; ++i) {
      auto upper_elem = elementwise_max(a[i], b[0]);
      if (upper_elem) result.emplace_back(*upper_elem);
    }
  } else {
    // incompatible size
    return std::nullopt;
  }

  return result;
}

/**
 * @brief Determines the element-wise min/max of two aggregate type values
 * by comparing each corresponding element.
 *
 * @tparam T The type of the aggregate values.
 * @param a The first aggregate.
 * @param b The second aggregate.
 * @return A new aggregate representing the element-wise min/max of the two
 * inputs, or `std::nullopt` if the element-wise comparison fails.
 *
 * Example:
 * struct Point {
 *   int x;
 *   int y;
 * };
 * Point p1{3, 5};
 * Point p2{4, 2};
 * auto result = elementwise_min(p1, p2);  // result will be Point{3, 2}
 * auto result = elementwise_max(p1, p2);  // result will be Point{4, 5}
 */
template <typename T>
  requires std::is_class_v<T> && std::is_aggregate_v<T>
[[nodiscard]]
std::optional<T> elementwise_min(const T& a, const T& b) {
  const auto elementwise_min_op = [](const auto& a_member, const auto& b_member) {
    return elementwise_min(a_member, b_member);
  };
  return op_aggregate(elementwise_min_op, a, b);
}

template <typename T>
  requires std::is_class_v<T> && std::is_aggregate_v<T>
[[nodiscard]]
std::optional<T> elementwise_max(const T& a, const T& b) {
  const auto elementwise_max_op = [](const auto& a_member, const auto& b_member) {
    return elementwise_max(a_member, b_member);
  };
  return op_aggregate(elementwise_max_op, a, b);
}

template <typename T>
  requires has_tag_and_get_tag_v<T>
[[nodiscard]]
std::optional<T> elementwise_min(const T& a, const T& b) {
  std::optional<T> ret = std::nullopt;
  auto elementwise_min_op = [&]<typename T::Tag TAG, typename P>(const P& p1, const P& p2) {
    auto p = elementwise_min(p1, p2);
    if (!p) return;
    ret = T::template make<TAG>(*p);
  };
  aidl_union_op(elementwise_min_op, a, b);
  return ret;
}

// handle the case of a sub union class inside another union
template <typename T>
  requires has_tag_and_get_tag_v<T>
[[nodiscard]]
std::optional<T> elementwise_max(const T& a, const T& b) {
  std::optional<T> ret = std::nullopt;
  auto elementwise_max_op = [&]<typename T::Tag TAG, typename P>(const P& p1, const P& p2) {
    auto p = elementwise_max(p1, p2);
    if (!p) return;
    ret = T::template make<TAG>(*p);
  };
  aidl_union_op(elementwise_max_op, a, b);
  return ret;
}

}  // namespace android::audio_utils

#endif  // __cplusplus