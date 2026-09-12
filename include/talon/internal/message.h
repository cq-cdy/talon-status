// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_INTERNAL_MESSAGE_H_
#define TALON_INTERNAL_MESSAGE_H_

#include <cstddef>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "talon/config.h"
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
#include <string_view>
#endif

namespace talon {
namespace internal {

template <typename T>
class IsStreamInsertable {
  template <typename U>
  static auto Test(int)
      -> decltype(std::declval<std::ostream&>() << std::declval<const U&>(),
                  std::true_type());
  template <typename>
  static std::false_type Test(...);

 public:
  static const bool value = decltype(Test<T>(0))::value;
};

inline void AppendMessage(std::ostream& stream, const char* value) {
  stream << (value == nullptr ? "(null)" : value);
}
inline void AppendMessage(std::ostream& stream, char* value) {
  AppendMessage(stream, static_cast<const char*>(value));
}
inline void AppendMessage(std::ostream& stream, std::nullptr_t) {
  stream << "(null)";
}
inline void AppendMessage(std::ostream& stream, signed char value) {
  stream << static_cast<int>(value);
}
inline void AppendMessage(std::ostream& stream, unsigned char value) {
  stream << static_cast<unsigned int>(value);
}
template <typename T>
typename std::enable_if<
    !std::is_convertible<const T&, const char*>::value>::type
AppendMessage(std::ostream& stream, const T& value) {
  static_assert(IsStreamInsertable<T>::value,
                "Talon message arguments must support stream insertion");
  stream << value;
}

inline std::string MakeMessage() { return std::string(); }

// Keep ostream formatting for any pack containing custom types, manipulators or
// floating-point values. Their formatting can affect subsequent arguments.
template <typename T>
struct IsDirectMessageArg {
  typedef typename std::decay<T>::type Value;
  static const bool value =
      (std::is_integral<Value>::value && !std::is_same<Value, wchar_t>::value &&
       !std::is_same<Value, char16_t>::value &&
       !std::is_same<Value, char32_t>::value
#if defined(__cpp_char8_t)
       && !std::is_same<Value, char8_t>::value
#endif
       ) ||
      std::is_same<Value, std::string>::value ||
      std::is_same<Value, char*>::value ||
      std::is_same<Value, const char*>::value ||
      std::is_same<Value, std::nullptr_t>::value
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
      || std::is_same<Value, std::string_view>::value
#endif
      ;
};

template <typename... Args>
struct AreDirectMessageArgs : std::true_type {};
template <typename First, typename... Rest>
struct AreDirectMessageArgs<First, Rest...>
    : std::integral_constant<bool, IsDirectMessageArg<First>::value &&
                                       AreDirectMessageArgs<Rest...>::value> {};

inline void AppendDirect(std::string& output, const std::string& value) {
  output.append(value);
}
inline void AppendDirect(std::string& output, const char* value) {
  output.append(value == nullptr ? "(null)" : value);
}
inline void AppendDirect(std::string& output, char* value) {
  AppendDirect(output, static_cast<const char*>(value));
}
inline void AppendDirect(std::string& output, std::nullptr_t) {
  output.append("(null)");
}
inline void AppendDirect(std::string& output, char value) {
  output.push_back(value);
}
inline void AppendDirect(std::string& output, bool value) {
  output.push_back(value ? '1' : '0');
}
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
inline void AppendDirect(std::string& output, std::string_view value) {
  if (!value.empty()) output.append(value.data(), value.size());
}
#endif

template <typename T>
bool IsNegative(T value, std::true_type) noexcept {
  return value < 0;
}
template <typename T>
bool IsNegative(T, std::false_type) noexcept {
  return false;
}
template <typename T>
typename std::enable_if<std::is_integral<T>::value &&
                        !std::is_same<T, char>::value &&
                        !std::is_same<T, bool>::value>::type
AppendDirect(std::string& output, T value) {
  typedef typename std::make_unsigned<T>::type Unsigned;
  Unsigned magnitude = static_cast<Unsigned>(value);
  const bool negative = IsNegative(value, std::is_signed<T>());
  // Unsigned subtraction handles the minimum signed value without overflow.
  if (negative) magnitude = static_cast<Unsigned>(Unsigned(0) - magnitude);
  // One more digit than digits10, a sign, and a spare byte; independent of
  // CHAR_BIT and the platform's integer widths.
  char digits[std::numeric_limits<Unsigned>::digits10 + 3];
  char* const end = digits + sizeof(digits);
  char* position = end;
  do {
    *--position = static_cast<char>('0' + magnitude % 10);
    magnitude = static_cast<Unsigned>(magnitude / 10);
  } while (magnitude != 0);
  if (negative) *--position = '-';
  output.append(position, static_cast<std::size_t>(end - position));
}

template <typename... Args>
std::string MakeMessageImpl(std::true_type, const Args&... args) {
  std::string output;
  const int sequence[] = {0, (AppendDirect(output, args), 0)...};
  (void)sequence;
  return output;
}

template <typename... Args>
std::string MakeMessageImpl(std::false_type, const Args&... args) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream.exceptions(std::ios::badbit | std::ios::failbit);
  // List-initialization sequences stream insertion in argument order in C++11.
  const int sequence[] = {0, (AppendMessage(stream, args), 0)...};
  (void)sequence;
  return stream.str();
}

template <typename... Args>
std::string MakeMessage(const Args&... args) {
  return MakeMessageImpl(AreDirectMessageArgs<Args...>(), args...);
}

}  // namespace internal
}  // namespace talon

#endif  // TALON_INTERNAL_MESSAGE_H_
