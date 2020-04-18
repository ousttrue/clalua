#pragma once
#include <string_view>

template <typename E, E V> std::string_view enum_name_impl() {
  //   constexpr auto str = __FUNCSIG__;
  constexpr auto prefix_len =
      sizeof("std::string_view __cdecl enum_name_impl(void) [E = CXCursorKind, "
             "V = ") -
      1;
  //   constexpr auto suffix_len = sizeof(">(void)");
  constexpr auto size = sizeof(__FUNCSIG__) - 1;
  auto sig = __FUNCSIG__;
  auto begin = sig + prefix_len;
  return std::string_view(begin, size - prefix_len - 1);
}

template <int N, typename E> struct enum_name_n {
  static std::string_view get(int n) {
    if (n > N) {
      return "oops...";
    } else if (n == N) {
      return enum_name_impl<E, E{static_cast<E>(N)}>();
    } else {
      return enum_name_n<N - 1, E>::get(n);
    }
  }
};

template <typename E> struct enum_name_n<0, E> {
  static std::string_view get(int n) {
    if (n == 0) {
      return enum_name_impl<E, E{static_cast<E>(0)}>();
    } else {
      return "oops...";
    }
  }
};

template <typename E> std::string_view enum_name(E e) {
  return enum_name_n<700, E>::get(e);
}

template <typename E, size_t MAX_VALUE> struct enum_name_map {
  std::array<std::string_view, MAX_VALUE + 1> _values;
  enum_name_map() {
    for (int i = 0; i < MAX_VALUE + 1; ++i) {
      _values[i] = enum_name_n<MAX_VALUE, E>::get(i);
    }
  }

  static std::string_view get(E e) {
    static enum_name_map s_map;
    return s_map._values[e];
  }
};
