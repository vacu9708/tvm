/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*!
 * \file tvm/ffi/base_details.h
 * \brief Internal detail utils that can be used by files in tvm/ffi.
 * \note details header are for internal use only
 *       and not to be directly used by user.
 */
#ifndef TVM_FFI_BASE_DETAILS_H_
#define TVM_FFI_BASE_DETAILS_H_

#include <tvm/ffi/c_api.h>
#include <tvm/ffi/endian.h>

#include <cstddef>
#include <utility>

#if defined(_MSC_VER)
#define TVM_FFI_INLINE __forceinline
#else
#define TVM_FFI_INLINE inline __attribute__((always_inline))
#endif

/*!
 * \brief Macro helper to force a function not to be inlined.
 * It is only used in places that we know not inlining is good,
 * e.g. some logging functions.
 */
#if defined(_MSC_VER)
#define TVM_FFI_NO_INLINE __declspec(noinline)
#else
#define TVM_FFI_NO_INLINE __attribute__((noinline))
#endif

#if defined(_MSC_VER)
#define TVM_FFI_UNREACHABLE() __assume(false)
#else
#define TVM_FFI_UNREACHABLE() __builtin_unreachable()
#endif

/*! \brief helper macro to suppress unused warning */
#if defined(__GNUC__)
#define TVM_FFI_ATTRIBUTE_UNUSED __attribute__((unused))
#else
#define TVM_FFI_ATTRIBUTE_UNUSED
#endif

#define TVM_FFI_STR_CONCAT_(__x, __y) __x##__y
#define TVM_FFI_STR_CONCAT(__x, __y) TVM_FFI_STR_CONCAT_(__x, __y)

#if defined(__GNUC__) || defined(__clang__)
#define TVM_FFI_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define TVM_FFI_FUNC_SIG __FUNCSIG__
#else
#define TVM_FFI_FUNC_SIG __func__
#endif

/*
 * \brief Define the default copy/move constructor and assign operator
 * \param TypeName The class typename.
 */
#define TVM_FFI_DEFINE_DEFAULT_COPY_MOVE_AND_ASSIGN(TypeName) \
  TypeName(const TypeName& other) = default;                  \
  TypeName(TypeName&& other) = default;                       \
  TypeName& operator=(const TypeName& other) = default;       \
  TypeName& operator=(TypeName&& other) = default;

namespace tvm {
namespace ffi {
namespace details {

/********** Atomic Operations *********/

TVM_FFI_INLINE int32_t AtomicIncrementRelaxed(int32_t* ptr) {
#ifdef _MSC_VER
  return _InterlockedIncrement(reinterpret_cast<volatile long*>(ptr)) - 1;  // NOLINT(*)
#else
  return __atomic_fetch_add(ptr, 1, __ATOMIC_RELAXED);
#endif
}

TVM_FFI_INLINE int32_t AtomicDecrementRelAcq(int32_t* ptr) {
#ifdef _MSC_VER
  return _InterlockedDecrement(reinterpret_cast<volatile long*>(ptr)) + 1;  // NOLINT(*)
#else
  return __atomic_fetch_sub(ptr, 1, __ATOMIC_ACQ_REL);
#endif
}

TVM_FFI_INLINE int32_t AtomicLoadRelaxed(const int32_t* ptr) {
  int32_t* raw_ptr = const_cast<int32_t*>(ptr);
#ifdef _MSC_VER
  // simply load the variable ptr out
  return (reinterpret_cast<const volatile long*>(raw_ptr))[0];  // NOLINT(*)
#else
  return __atomic_load_n(raw_ptr, __ATOMIC_RELAXED);
#endif
}

// for each iterator
template <bool stop, std::size_t I, typename F>
struct for_each_dispatcher {
  template <typename T, typename... Args>
  static void run(const F& f, T&& value, Args&&... args) {  // NOLINT(*)
    f(I, std::forward<T>(value));
    for_each_dispatcher<sizeof...(Args) == 0, (I + 1), F>::run(f, std::forward<Args>(args)...);
  }
};

template <std::size_t I, typename F>
struct for_each_dispatcher<true, I, F> {
  static void run(const F&) {}  // NOLINT(*)
};

template <typename F, typename... Args>
void for_each(const F& f, Args&&... args) {  // NOLINT(*)
  for_each_dispatcher<sizeof...(Args) == 0, 0, F>::run(f, std::forward<Args>(args)...);
}

/*!
 * \brief Hash the binary bytes
 * \param data The data pointer
 * \param size The size of the bytes.
 * \return the hash value.
 */
TVM_FFI_INLINE uint64_t StableHashBytes(const char* data, size_t size) {
  const constexpr uint64_t kMultiplier = 1099511628211ULL;
  const constexpr uint64_t kMod = 2147483647ULL;
  union Union {
    uint8_t a[8];
    uint64_t b;
  } u;
  static_assert(sizeof(Union) == sizeof(uint64_t), "sizeof(Union) != sizeof(uint64_t)");
  const char* it = data;
  const char* end = it + size;
  uint64_t result = 0;
  for (; it + 8 <= end; it += 8) {
    if (TVM_FFI_IO_NO_ENDIAN_SWAP) {
      u.a[0] = it[0];
      u.a[1] = it[1];
      u.a[2] = it[2];
      u.a[3] = it[3];
      u.a[4] = it[4];
      u.a[5] = it[5];
      u.a[6] = it[6];
      u.a[7] = it[7];
    } else {
      u.a[0] = it[7];
      u.a[1] = it[6];
      u.a[2] = it[5];
      u.a[3] = it[4];
      u.a[4] = it[3];
      u.a[5] = it[2];
      u.a[6] = it[1];
      u.a[7] = it[0];
    }
    result = (result * kMultiplier + u.b) % kMod;
  }
  if (it < end) {
    u.b = 0;
    uint8_t* a = u.a;
    if (it + 4 <= end) {
      a[0] = it[0];
      a[1] = it[1];
      a[2] = it[2];
      a[3] = it[3];
      it += 4;
      a += 4;
    }
    if (it + 2 <= end) {
      a[0] = it[0];
      a[1] = it[1];
      it += 2;
      a += 2;
    }
    if (it + 1 <= end) {
      a[0] = it[0];
      it += 1;
      a += 1;
    }
    if (!TVM_FFI_IO_NO_ENDIAN_SWAP) {
      std::swap(u.a[0], u.a[7]);
      std::swap(u.a[1], u.a[6]);
      std::swap(u.a[2], u.a[5]);
      std::swap(u.a[3], u.a[4]);
    }
    result = (result * kMultiplier + u.b) % kMod;
  }
  return result;
}
}  // namespace details
}  // namespace ffi
}  // namespace tvm
#endif  // TVM_FFI_BASE_DETAILS_H_
