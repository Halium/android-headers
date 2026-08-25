/*
 * Copyright (C) 2023 The Android Open Source Project
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
#include <aidl/android/hardware/audio/effect/AcousticEchoCanceler.h>
#include <aidl/android/hardware/audio/effect/Capability.h>
#include <aidl/android/hardware/audio/effect/DynamicsProcessing.h>
#include <aidl/android/hardware/audio/effect/Parameter.h>
#include <aidl/android/hardware/audio/effect/Range.h>
#include <audio_utils/template_utils.h>
#include <system/elementwise_op.h>

#include <optional>
#include <set>

namespace aidl::android::hardware::audio::effect {

/**
 * The first AIDL version that introduced the IEffect::reopen method.
 */
static constexpr int32_t kReopenSupportedVersion = 2;

/**
 * The first AIDL version that introduced the android.hardware.audio.effect.State.DRAINING state.
 */
static constexpr int32_t kDrainSupportedVersion = 3;
/**
 * The first AIDL version that support effect destroy at any state.
 */
static constexpr int32_t kDestroyAnyStateSupportedVersion = 3;

/**
 * EventFlag to indicate that the client has written data to the FMQ, align with
 * EffectHalAidl.
 *
 * This flag is deprecated start from HAL AIDL version 2 and should not be used.
 * Bit 0x01 and 0x02 were used by FMQ internally (FMQ_NOT_FULL and
 * FMQ_NOT_EMPTY), using these event flag bits will cause conflict and may
 * result in a waiter not able to receive wake correctly.
 */
static constexpr uint32_t kEventFlagNotEmpty = 0x1;
/**
 * EventFlag for the effect instance to indicate that the data FMQ needs to be updated.
 * TODO: b/277900230, Define in future AIDL version.
 */
static constexpr uint32_t kEventFlagDataMqUpdate = 0x1 << 10;
/**
 * EventFlag to indicate that the data FMQ is not Empty after a write.
 * TODO: b/277900230, Define in future AIDL version.
 */
static constexpr uint32_t kEventFlagDataMqNotEmpty = 0x1 << 11;

/**
 * Check the target Parameter with $Parameter$Range definition in Capability.
 * This method go through the elements in the ranges to find a matching tag for the target
 * parameter, and check if the target parameter is inside range use the default AIDL union
 * comparator.
 *
 * Absence of a corresponding range is an indication that there are no limits set on the parameter
 * so this method return true.
 */
template <typename T, typename R>
static inline bool inRange(const T& target, const R& ranges) {
  for (const auto& r : ranges) {
    if (target.getTag() == r.min.getTag() &&
        target.getTag() == r.max.getTag() &&
        (target < r.min || target > r.max)) {
      return false;
    }
  }
  return true;
}

template <typename Range::Tag rangeTag, typename T>
static inline bool inRange(const T& target, const Capability& cap) {
  if (cap.range.getTag() == rangeTag) {
    const auto& ranges = cap.range.template get<rangeTag>();
    return inRange(target, ranges);
  }
  return true;
}

/**
 * Return the range pair (as defined in aidl::android::hardware::audio::effect::Range) of a
 * parameter.
 */
template <typename Range::Tag RangeTag, typename R, typename T>
static inline std::optional<R> getRange(const Capability& cap, T tag) {
  if (cap.range.getTag() != RangeTag) {
    return std::nullopt;
  }

  const auto& ranges = cap.range.template get<RangeTag>();
  for (const auto& r : ranges) {
    if (r.min.getTag() == tag && r.max.getTag() == tag) {
      return r;
    }
  }

  return std::nullopt;
}

template <typename T, typename R>
static inline bool isRangeValid(const T& tag, const R& ranges) {
  for (const auto& r : ranges) {
    if (tag == r.min.getTag() && tag == r.max.getTag()) {
      return r.min <= r.max;
    }
  }

  return true;
}

template <typename Range::Tag rangeTag, typename T>
static inline bool isRangeValid(const T& paramTag, const Capability& cap) {
  if (cap.range.getTag() == rangeTag) {
    const auto& ranges = cap.range.template get<rangeTag>();
    return isRangeValid(paramTag, ranges);
  }
  return true;
}

/**
 * @brief Clamps a parameter to its valid range with `android::audio_utils::elementwise_clamp`.
 *
 * @tparam RangeTag, `Range::dynamicsProcessing` for example.
 * @tparam SpecificTag The effect specific tag in Parameter,
 *         `Parameter::Specific::dynamicsProcessing` for example.
 * @param param The parameter to clamp, `DynamicsProcessing` for example.
 * @param cap The effect capability.
 * @return Return the clamped parameter on success, `std::nullopt` on any failure.
 */
template <Range::Tag RangeTag, Parameter::Specific::Tag SpecificTag>
[[nodiscard]]
static inline std::optional<Parameter> clampParameter(const Parameter& param,
                                                      const Capability& cap) {
  if constexpr (RangeTag == Range::vendorExtension) return std::nullopt;

  // field tag must matching to continue
  if (param.getTag() != Parameter::specific) return std::nullopt;

  Parameter::Specific specific = param.template get<Parameter::specific>();
  auto effect = specific.template get<SpecificTag>();
  std::optional<decltype(effect)> clamped = std::nullopt;

  const Range& range = cap.range;
  // no need to clamp if the range capability not defined
  if (range.getTag() != RangeTag) return param;

  const auto& ranges = range.template get<RangeTag>();
  for (const auto& r : ranges) {
    clamped = ::android::audio_utils::elementwise_clamp(effect, r.min, r.max);
    if (clamped != std::nullopt) {
      if (effect != clamped.value()) {
        ALOGI("%s from \"%s\" to \"%s\"", __func__, effect.toString().c_str(),
              clamped->toString().c_str());
      }
      break;
    }
  }

  if (clamped == std::nullopt) return std::nullopt;
  return Parameter::make<Parameter::specific>(
      Parameter::Specific::make<SpecificTag>(clamped.value()));
}

/**
 * Customized comparison for AIDL effect Range classes, the comparison is based on the tag value of
 * the class.
 * `VendorExtensionRange` is special because the underlying `VendorExtension` is not an AIDL union,
 * so we compare the value directly.
 */
template <typename T>
struct RangeTagLessThan {
  bool operator()(const T& a, const T& b) const {
    if constexpr (std::is_same_v<T, Range::VendorExtensionRange>) return a < b;
    else return a.min.getTag() < b.min.getTag();
  }
};

/**
 * @brief Find the shared capability of two capabilities `cap1` and `cap2`.
 * A shared range is the intersection part of these two capabilities.
 *
 * For example, for below capabilities:
 * Capability cap1 = {.range = Range::make<Range::volume>({MAKE_RANGE(Volume, levelDb, -4800, 0)})};
 * Capability cap2 = {.range = Range::make<Range::volume>({MAKE_RANGE(Volume, levelDb, -9600,
 *                    -1600)})};
 * Capability cap3 = {.range = Range::make<Range::volume>({MAKE_RANGE(Volume, levelDb, -800, 0)})};
 *
 * The shared capability of cap1 and cap2 is:
 * Capability{.range = Range::make<Range::volume>({MAKE_RANGE(Volume, levelDb, -4800, -1600)})};
 * The shared capability of cap1 and cap3 is:
 * Capability{.range = Range::make<Range::volume>({MAKE_RANGE(Volume, levelDb, -800, 0)})};
 * The shared capability of cap2 and cap3 is empty so `findSharedCapability` return std::nullopt.
 *
 * @param cap1 The first capability
 * @param cap2 The second capability
 * @return The shared capability on success, std::nullopt on any failure.
 */
 [[nodiscard]]
static inline std::optional<Capability> findSharedCapability(
    const Capability& cap1, const Capability& cap2) {
  if (cap1.range.getTag() != cap2.range.getTag()) return std::nullopt;

  std::optional<Capability> sharedCap = std::nullopt;
  // RangeTag: tag id of the Effect range, `Range::dynamicsProcessing` for example.
  // T: type of the effect range, `DynamicsProcessingRange` for example.
  auto overlapRangeFinder = [&]<Range::Tag RangeTag, typename T>(
                                const std::vector<T>& vec1,
                                const std::vector<T>& vec2) {
    if constexpr (RangeTag == Range::vendorExtension) {
      sharedCap = {.range = Range::make<RangeTag>(vec1)};
      return;
    }

    if (vec1.empty()) {
      sharedCap = {.range = Range::make<RangeTag>(vec2)};
      return;
    }
    if (vec2.empty()) {
      sharedCap = {.range = Range::make<RangeTag>(vec1)};
      return;
    }

    std::vector<T> sharedVec;
    std::set<T, RangeTagLessThan<T>> set2{vec2.begin(), vec2.end()};
    std::for_each(vec1.begin(), vec1.end(), [&](const auto& v1) {
      const auto& v2 = set2.find(v1);
      if (v2 != set2.end()) {
        auto min = ::android::audio_utils::elementwise_max(v1.min, v2->min);
        auto max = ::android::audio_utils::elementwise_min(v1.max, v2->max);
        // only add range to vector when at least min or max is valid
        if (min != std::nullopt || max != std::nullopt) {
          using ElementType = decltype(v1.min);
          sharedVec.emplace_back(T{.min = min.value_or(ElementType{}),
                                   .max = max.value_or(ElementType{})});
        }
      }
    });
    if (!sharedVec.empty()) sharedCap = {.range = Range::make<RangeTag>(sharedVec)};
  };

  // find the underlying value in these two ranges, and call `overlapRangeFinder` lambda
  ::android::audio_utils::aidl_union_op(overlapRangeFinder, cap1.range, cap2.range);
  return sharedCap;
}

}  // namespace aidl::android::hardware::audio::effect