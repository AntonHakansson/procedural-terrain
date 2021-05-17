#pragma once

#include <stdint.h>

#include <cstddef>

// Base Types
//-----------------------------------------------
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = i8;
using s16 = i16;
using s32 = i32;
using s64 = i64;
using b8 = i8;
using b16 = i16;
using b32 = i32;
using b64 = i64;
using f32 = float;
using f64 = double;
using usize = size_t;
using isize = std::ptrdiff_t;

// Base Constants
//-----------------------------------------------
// Sometimes it exists, sometimes not...
#ifndef M_PI
#  define M_PI 3.14159265358979323846f
#endif

// Defer statements
//-----------------------------------------------
namespace {
  template <typename T> struct gbRemoveReference { using Type = T; };
  template <typename T> struct gbRemoveReference<T &> { using Type = T; };
  template <typename T> struct gbRemoveReference<T &&> { using Type = T; };

  template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &t) {
    return static_cast<T &&>(t);
  }
  template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &&t) {
    return static_cast<T &&>(t);
  }
  template <typename T> inline T &&gb_move(T &&t) {
    return static_cast<typename gbRemoveReference<T>::Type &&>(t);
  }
  template <typename F> struct gbprivDefer {
    F f;
    gbprivDefer(F &&f) : f(gb_forward<F>(f)) {}
    ~gbprivDefer() { f(); }
  };
  template <typename F> gbprivDefer<F> gb__defer_func(F &&f) {
    return gbprivDefer<F>(gb_forward<F>(f));
  }

#define GB_DEFER_1(x, y) x##y
#define GB_DEFER_2(x, y) GB_DEFER_1(x, y)
#define GB_DEFER_3(x) GB_DEFER_2(x, __COUNTER__)
#define defer(code) auto GB_DEFER_3(_defer_) = gb__defer_func([&]() -> void { code; })
}  // namespace
