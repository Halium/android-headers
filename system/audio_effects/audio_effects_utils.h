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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <type_traits>
#include <utils/Errors.h>

#include <system/audio_effect.h>

namespace android {
namespace effect {
namespace utils {

/**
 * A wrapper class of legacy effect_param_t to avoid raw pointer read/write everywhere.
 * The wrapper use the reference of underlying effect_param_t structure, and does not manage
 * ownership of the structure.
 * Thread safety is not in consideration in this class for now.
 */
class EffectParamWrapper {
 public:
  explicit EffectParamWrapper(const effect_param_t& param) : mParam(param) {}

  // validate command size to be at least parameterSize + valueSize after effect_param_t
  bool validateCmdSize(size_t cmdSize) const {
    return (uint64_t)getPaddedParameterSize() + mParam.vsize + sizeof(effect_param_t) <=
           cmdSize;
  }

  /**
   *  validate parameter and value size to be at least equals to the target size.
   */
  bool validateParamValueSize(size_t paramSize, size_t valueSize) const {
    return mParam.psize >= paramSize && mParam.vsize >= valueSize;
  }

  std::string toString() const {
    std::ostringstream os;
    os << "effect_param_t: { ";
    os << "status: " << mParam.status << ", p: " << mParam.psize
       << " (padded: " << getPaddedParameterSize() << "), v: " << mParam.vsize
       << ", dataAddr: " << &mParam.data;
    os << "}";
    return os.str();
  }

  status_t copyDataWithCheck(void* dst, const void* src, size_t len,
                                    size_t offset, size_t max) {
    if (!dst || !src || len + offset > max) {
      return BAD_VALUE;
    }
    std::memcpy(dst, src, len);
    return OK;
  }

  status_t readFromData(void* buf, size_t len, size_t offset, size_t max) {
    return copyDataWithCheck(buf, mParam.data + offset, len, offset, max);
  }

  status_t getStatus() const { return mParam.status; }
  size_t getPaddedParameterSize() const { return padding(mParam.psize); }
  size_t getParameterSize() const { return mParam.psize; }
  size_t getValueSize() const { return mParam.vsize; }
  const uint8_t* getValueAddress() const {
    return (uint8_t*)mParam.data + getPaddedParameterSize();
  }

  uint64_t getTotalSize() const {
    return (uint64_t)sizeof(effect_param_t) + getPaddedParameterSize() + getValueSize();
  }

  /**
   * Get reference to effect_param_t.
   */
  const effect_param_t& getEffectParam() const { return mParam; }

  bool operator==(const EffectParamWrapper& other) const {
    return (&other == this) || 0 == std::memcmp(&mParam, &other.mParam, sizeof(effect_param_t));
  }

  /**
   * Padding psize to 32 bits aligned, because "start of value field inside
   * the data field is always on a 32 bits boundary".
   */
  static constexpr inline size_t padding(size_t size) {
    return ((size - 1) / sizeof(int32_t) + 1) * sizeof(int32_t);
  }

 private:
  /* member with variable sized type at end of class */
  const effect_param_t& mParam;
};

/**
 * Reader class of effect_param_t data buffer.
 */
class EffectParamReader : public EffectParamWrapper {
 public:
  explicit EffectParamReader(const effect_param_t& param)
      : EffectParamWrapper(param), mValueROffset(getPaddedParameterSize()) {}

  /**
   * Read n value of type T from data to buf, mParamROffset will advance n * sizeof(T) if success,
   * no advance if failure.
   */
  template <typename T>
  status_t readFromParameter(T* buf, size_t n = 1) {
    size_t len = n * sizeof(T);
    status_t ret = readFromData(buf, len, mParamROffset /* param offset */,
                                getParameterSize() /* max offset */);
    if (OK == ret) {
       mParamROffset += len;
    }
    return ret;
  }

  /**
   * Read number of value in type T from data to buf, mValueROffset will advance n * sizeof(T) if
   * success, no advance if failure.
   */
  template <typename T>
  status_t readFromValue(T* buf, size_t n = 1) {
    size_t len = n * sizeof(T);
    status_t ret = readFromData(buf, len, mValueROffset /* data offset */,
                                getPaddedParameterSize() + getValueSize() /* max offset */);
    if (OK == ret) {
       mValueROffset += len;
    }
    return ret;
  }

  std::string toString() const {
    std::ostringstream os;
    os << EffectParamWrapper::toString();
    os << ", paramROffset: " << mParamROffset;
    os << ", valueROffset: " << mValueROffset;
    return os.str();
  }

  void reset() {
    mParamROffset = 0;
    mValueROffset = getPaddedParameterSize();
  }

 private:
  size_t mParamROffset = 0;
  size_t mValueROffset = 0;
};

/**
 * Writer class of effect_param_t data buffer.
 */
class EffectParamWriter : public EffectParamReader {
 public:
  explicit EffectParamWriter(effect_param_t& param)
      : EffectParamReader(param), mParam(param), mValueWOffset(getPaddedParameterSize()) {
  }

  /**
   * Write n number of param in type T from buf to data, mParamWOffset will advance n * sizeof(T)
   * if success, no advance if failure.
   */
  template <typename T>
  status_t writeToParameter(const T* buf, size_t n = 1) {
    size_t len = n * sizeof(T);
    status_t ret = writeToData(buf, len, mParamWOffset /* data offset */,
                               getParameterSize() /* max */);
    if (OK == ret) {
       mParamWOffset += len;
    }
    return ret;
  }

  status_t writeToData(const void* buf, size_t len, size_t offset, size_t max) {
    return copyDataWithCheck(mParam.data + offset, buf, len, offset, max);
  }
  /**
   * Write n number of value in type T from buf to data, mValueWOffset will advance n * sizeof(T)
   * if success, no advance if failure.
   */
  template <typename T>
  status_t writeToValue(const T* buf, size_t n = 1) {
    size_t len = n * sizeof(T);
    status_t ret = writeToData(buf, len, mValueWOffset /* data offset */,
                               getPaddedParameterSize() + getValueSize() /* max */);
    if (OK == ret) {
       mValueWOffset += len;
    }
    return ret;
  }

  /**
   * Set the current value write offset to vsize.
   * Together with getTotalSize(), can be used by getParameter to set replySize.
   */
  void finishValueWrite() { mParam.vsize = mValueWOffset - getPaddedParameterSize(); }

  void setStatus(status_t status) { mParam.status = status; }

  /**
   * Overwrite the entire effect_param_t with input.
   */
  status_t overwrite(const effect_param_t& param) {
    EffectParamReader reader(param);
    const auto size = reader.getTotalSize();
    if (size > getTotalSize()) {
       return BAD_VALUE;
    }
    std::memcpy((void *)&mParam, (void *)&reader.getEffectParam(), size);
    reset();
    mValueWOffset += reader.getValueSize();
    return OK;
  }

  /**
   * Reset the offsets with underlying effect_param_t.
   */
  void reset() {
    EffectParamReader::reset();
    mParamWOffset = 0;
    mValueWOffset = getPaddedParameterSize();
  }

  std::string toString() const {
    std::ostringstream os;
    os << EffectParamReader::toString();
    os << ", paramWOffset: " << mParamWOffset;
    os << ", valueWOffset: " << mValueWOffset;
    return os.str();
  }

 private:
  effect_param_t& mParam;
  size_t mParamWOffset = 0;
  size_t mValueWOffset = 0;
};

inline std::string ToString(const audio_uuid_t& uuid) {
    char str[64];
    snprintf(str, sizeof(str), "%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
             uuid.timeLow, uuid.timeMid, uuid.timeHiAndVersion, uuid.clockSeq,
             uuid.node[0], uuid.node[1], uuid.node[2], uuid.node[3],
             uuid.node[4], uuid.node[5]);
    return str;
}

inline bool operator==(const audio_uuid_t& lhs, const audio_uuid_t& rhs) {
  return lhs.timeLow == rhs.timeLow && lhs.timeMid == rhs.timeMid &&
         lhs.timeHiAndVersion == rhs.timeHiAndVersion &&
         lhs.clockSeq == rhs.clockSeq &&
         std::memcmp(lhs.node, rhs.node, sizeof(lhs.node)) == 0;
}

inline bool operator!=(const audio_uuid_t& lhs, const audio_uuid_t& rhs) {
    return !(lhs == rhs);
}

/**
 * @brief Helper function to write a single parameter (type P) and value (type
 * V) to effect_param_t, with optional buffer size check.
 *
 * Type P and V must be trivially copyable type to ensure safe copying to the
 * effect_param_t structure.
 *
 * Usage:
 *   effect_param_t *param = (effect_param_t *)buf;
 *   if (OK != android::effect::utils::writeToEffectParam(param, p, v)) {
 *       // error handling
 *   }
 *
 * @param param The pointer to effect_param_t buffer.
 * @param p The parameter to write into effect_param_t, 32 bit padded.
 * @param v The value to write into effect_param_t, start of value field inside
 * the data field is always on a 32 bits boundary.
 * @param bufSize OPTIONAL: The size of the buffer pointer to effect_param_t. If
 * a valid bufSize provide, it will be used to verify if it's big enough to
 * write both param and value.
 * @return status_t OK on success, BAD_VALUE on any failure.
 * Specifically, BAD_VALUE is returned if:
 * - The `param` pointer is null.
 * - The `bufSize` is provided and is insufficient to hold the data.
 */
template <typename P, typename V>
  requires(std::is_trivially_copyable_v<P> && std::is_trivially_copyable_v<V>)
status_t writeToEffectParam(effect_param_t* param, const P p, const V v,
                            size_t bufSize = 0) {
  const size_t pSize = EffectParamWrapper::padding(sizeof(P)),
               vSize = sizeof(V);
  if (!param ||
      (bufSize != 0 && bufSize < sizeof(effect_param_t) + pSize + vSize)) {
    return BAD_VALUE;
  }

  param->psize = pSize;
  param->vsize = vSize;
  EffectParamWriter writer(*param);

  status_t ret = writer.writeToParameter(&p);
  return ret == OK ? writer.writeToValue(&v) : ret;
}

/**
 * @brief Helper function to read a single parameter (type P) and value (type V)
 * from effect_param_t.
 *
 * Type P and V must be trivially copyable type to ensure safe copying from the
 * effect_param_t structure.
 *
 * Usage:
 *   effect_param_t *param = (effect_param_t *)buf;
 *   if (OK != android::effect::utils::readFromEffectParam(param, &p, &v)) {
 *       // error handling
 *   }
 *
 * @param param The pointer to effect_param_t buffer.
 * @param p The pointer to the return parameter read from effect_param_t.
 * @param v The pointer to the return value read from effect_param_t.
 * @return status_t OK on success, BAD_VALUE on any failure.
 * Specifically, BAD_VALUE is returned if:
 * - Any of `param`, `p`, or `v` pointers is null.
 * - The `psize` or `vsize` is smaller than the size of `P` and `V`.
 *
 * **Important:** Even in case of an error (return value `BAD_VALUE`), the
 * memory location pointed to by `p` might be updated.
 */
template <typename P, typename V>
  requires(std::is_trivially_copyable_v<P> && std::is_trivially_copyable_v<V>)
status_t readFromEffectParam(const effect_param_t* param, P* p, V* v) {
  if (!param || !p || !v) return BAD_VALUE;

  const size_t pSize = sizeof(P), vSize = sizeof(V);
  EffectParamReader reader(*param);
  if (!reader.validateParamValueSize(pSize, vSize)) return BAD_VALUE;

  status_t ret = reader.readFromParameter(p);
  return ret == OK ? reader.readFromValue(v) : ret;
}

}  // namespace utils
}  // namespace effect
}  // namespace android
