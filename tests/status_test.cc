// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/status.h"

#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
#include <string_view>
#endif

#include "test.h"

namespace {
struct Item {
  int id;
};
std::ostream& operator<<(std::ostream& stream, const Item& item) {
  return stream << "item=" << item.id;
}
struct BrokenFormat {};
std::ostream& operator<<(std::ostream& stream, const BrokenFormat&) {
  stream.setstate(std::ios::failbit);
  return stream;
}

struct HexFormat {};
std::ostream& operator<<(std::ostream& stream, const HexFormat&) {
  return stream << std::hex << std::boolalpha;
}
struct StreamMode {};
std::ostream& operator<<(std::ostream& stream, const StreamMode&) {
  return stream;
}
struct GroupedNumbers : std::numpunct<char> {
  char do_thousands_sep() const override { return '_'; }
  std::string do_grouping() const override { return "\3"; }
};

template <typename T>
void CheckIntegral(T value) {
  std::ostringstream reference;
  reference.imbue(std::locale::classic());
  // signed/unsigned char are documented as numbers, not ostream characters.
  if (std::is_signed<T>::value) {
    reference << "number=" << static_cast<long long>(value) << "/";
  } else {
    reference << "number=" << static_cast<unsigned long long>(value) << "/";
  }
  CHECK_EQ(talon::InternalError("number=", value, "/").message(),
           reference.str());
  CHECK_EQ(talon::InternalError(StreamMode(), "number=", value, "/").message(),
           reference.str());
}

template <typename T>
void CheckIntegralBoundaries() {
  CheckIntegral(std::numeric_limits<T>::min());
  CheckIntegral(std::numeric_limits<T>::max());
  CheckIntegral(static_cast<T>(0));
  CheckIntegral(static_cast<T>(1));
}

void TestDirectMessageEquivalence() {
  CheckIntegralBoundaries<signed char>();
  CheckIntegralBoundaries<unsigned char>();
  CheckIntegralBoundaries<short>();
  CheckIntegralBoundaries<unsigned short>();
  CheckIntegralBoundaries<int>();
  CheckIntegralBoundaries<unsigned int>();
  CheckIntegralBoundaries<long>();
  CheckIntegralBoundaries<unsigned long>();
  CheckIntegralBoundaries<long long>();
  CheckIntegralBoundaries<unsigned long long>();
  for (int value = -10000; value <= 10000; ++value) CheckIntegral(value);
  CHECK_EQ(talon::InternalError(HexFormat(), 255, '/', true).message(),
           "ff/true");
  char* mutable_null = nullptr;
  const char* const_null = nullptr;
  CHECK_EQ(talon::InternalError(StreamMode(), mutable_null, const_null, nullptr,
                                false, '\0', std::string("a\0b", 3))
               .message(),
           std::string("(null)(null)(null)0\0a\0b", 23));
  CHECK_EQ(talon::InternalError(std::string("a\0b", 3), '\0', -42).message(),
           std::string("a\0b\0-42", 7));
  const std::locale original = std::locale();
  std::locale::global(std::locale(original, new GroupedNumbers));
  const std::string direct = talon::InternalError("n=", 1234567).message();
  const std::string fallback = talon::InternalError(Item{1234567}).message();
  std::locale::global(original);
  CHECK_EQ(direct, "n=1234567");
  CHECK_EQ(fallback, "item=1234567");
}

void TestCodes() {
  using talon::StatusCode;
  const char* names[] = {"OK",
                         "CANCELLED",
                         "UNKNOWN",
                         "INVALID_ARGUMENT",
                         "DEADLINE_EXCEEDED",
                         "NOT_FOUND",
                         "ALREADY_EXISTS",
                         "PERMISSION_DENIED",
                         "RESOURCE_EXHAUSTED",
                         "FAILED_PRECONDITION",
                         "ABORTED",
                         "OUT_OF_RANGE",
                         "UNIMPLEMENTED",
                         "INTERNAL",
                         "UNAVAILABLE",
                         "DATA_LOSS",
                         "UNAUTHENTICATED"};
  for (int i = 0; i != 17; ++i) {
    const talon::Status status(static_cast<StatusCode>(i), "detail");
    CHECK_EQ(static_cast<int>(status.code()), i);
    CHECK_EQ(status.ok(), i == 0);
    CHECK_EQ(status.message(), i == 0 ? "" : "detail");
    CHECK_EQ(std::string(talon::StatusCodeToString(status.code())), names[i]);
    CHECK_EQ(status.ToString(),
             i == 0 ? "OK" : std::string(names[i]) + ": detail");
  }
  const talon::Status factories[] = {talon::OkStatus(),
                                     talon::CancelledError(),
                                     talon::UnknownError(),
                                     talon::InvalidArgumentError(),
                                     talon::DeadlineExceededError(),
                                     talon::NotFoundError(),
                                     talon::AlreadyExistsError(),
                                     talon::PermissionDeniedError(),
                                     talon::ResourceExhaustedError(),
                                     talon::FailedPreconditionError(),
                                     talon::AbortedError(),
                                     talon::OutOfRangeError(),
                                     talon::UnimplementedError(),
                                     talon::InternalError(),
                                     talon::UnavailableError(),
                                     talon::DataLossError(),
                                     talon::UnauthenticatedError()};
  for (int i = 0; i != 17; ++i) {
    CHECK_EQ(static_cast<int>(factories[i].code()), i);
    CHECK_EQ(factories[i].message(), "");
    CHECK_EQ(factories[i].ToString(), names[i]);
  }
  for (int bad : {-1, 17, 999}) {
    const talon::Status status(static_cast<StatusCode>(bad), "unknown code");
    CHECK_EQ(status.code(), StatusCode::kUnknown);
    CHECK_EQ(status.message(), "unknown code");
    CHECK_EQ(
        std::string(talon::StatusCodeToString(static_cast<StatusCode>(bad))),
        "UNKNOWN");
  }
}

void TestOwnership() {
  talon::Status success;
  CHECK(success.ok());
  CHECK_EQ(success, talon::OkStatus());
  std::string text(256, 'x');
  talon::Status original = talon::NotFoundError(text);
  text[0] = 'y';
  CHECK_EQ(original.message()[0], 'x');
  talon::Status copied(original);
  CHECK_EQ(copied, original);
  talon::Status moved(std::move(original));
  CHECK_EQ(moved, copied);
  CHECK_EQ(original.code(), talon::StatusCode::kNotFound);
  CHECK(original.message().empty());
  success = moved;
  CHECK_EQ(success, copied);
  success = std::move(moved);
  CHECK_EQ(success, copied);
  CHECK_EQ(moved.code(), talon::StatusCode::kNotFound);
  CHECK(moved.message().empty());
  talon::Status* alias = &success;
  success = *alias;
  success = std::move(*alias);
  CHECK_EQ(success, copied);
  talon::Status empty;
  swap(empty, success);
  CHECK(success.ok());
  CHECK(success.message().empty());
  CHECK_EQ(empty, copied);
  CHECK(empty != success);
  talon::Status moved_ok(std::move(success));
  CHECK(moved_ok.ok() && success.ok());
  CHECK(moved_ok.message().empty() && success.message().empty());
  std::ostringstream stream;
  stream << copied;
  CHECK_EQ(stream.str(), copied.ToString());
}

void TestMessages() {
  CHECK_EQ(talon::InvalidArgumentError("invalid size: ", 2, ", expected: ", 7)
               .message(),
           "invalid size: 2, expected: 7");
  const char* const_null = nullptr;
  char* mutable_null = nullptr;
  CHECK_EQ(talon::InternalError(const_null, '/', mutable_null, '/', nullptr)
               .message(),
           "(null)/(null)/(null)");
  CHECK_EQ(talon::Status(talon::StatusCode::kInternal, const_null).message(),
           "(null)");
  CHECK(talon::Status(talon::StatusCode::kOk, const_null).message().empty());
  CHECK_EQ(talon::InternalError("", std::string(), std::string("temporary"))
               .message(),
           "temporary");
  char buffer[] = "owned";
  const talon::Status owned = talon::InternalError(buffer);
  buffer[0] = 'X';
  CHECK_EQ(owned.message(), "owned");
  CHECK_EQ(talon::InternalError(std::string("a\0b", 3)).message(),
           std::string("a\0b", 3));
  CHECK_EQ(talon::InternalError(true, '/', false, '/', 'x', '/',
                                static_cast<signed char>(-8), '/',
                                static_cast<unsigned char>(250))
               .message(),
           "1/0/x/-8/250");
  CHECK_EQ(talon::InternalError(1.25, '/', 1.25F, '/', 1.25L).message(),
           "1.25/1.25/1.25");
  CHECK_EQ(talon::InternalError(std::numeric_limits<unsigned long long>::max())
               .message(),
           "18446744073709551615");
  CHECK_EQ(talon::InternalError(Item{42}).message(), "item=42");
  talon_test::CheckThrows<std::ios_base::failure>(
      [] { (void)talon::InternalError(BrokenFormat()); });
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
  std::string source("view\0data", 9);
  const talon::Status view_status =
      talon::InternalError(std::string_view(source));
  source.clear();
  CHECK_EQ(view_status.message(), std::string("view\0data", 9));
  CHECK(talon::InternalError(std::string_view()).message().empty());
#endif
}
}  // namespace

int main() {
  TestCodes();
  TestOwnership();
  TestMessages();
  TestDirectMessageEquivalence();
  return 0;
}
