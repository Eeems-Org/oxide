// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_BASE_CONTAINERS_SPAN_H_
#define MINI_CHROMIUM_BASE_CONTAINERS_SPAN_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/dynamic_extent.h"
#include "base/numerics/safe_conversions.h"
#include "base/template_util.h"
#include "base/types/to_address.h"

// No absl in mini_chromium.
#if !defined(ABSL_ATTRIBUTE_LIFETIME_BOUND)
#define ABSL_ATTRIBUTE_LIFETIME_BOUND
#endif

namespace base {

template <typename T,
          size_t Extent = dynamic_extent,
          typename InternalPtrType = T*>
class span;

namespace internal {

template <typename From, typename To>
concept LegalDataConversion =
    std::convertible_to<std::remove_reference_t<From> (*)[],
                        std::remove_reference_t<To> (*)[]>;

template <typename T, typename It>
concept CompatibleIter = std::contiguous_iterator<It> &&
                         LegalDataConversion<std::iter_reference_t<It>, T>;

template <typename T, typename R>
concept CompatibleRange =
    std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
    LegalDataConversion<std::ranges::range_reference_t<R>, T> &&
    (std::ranges::borrowed_range<R> || std::is_const_v<T>);

// NOTE: Ideally we'd just use `CompatibleRange`, however this currently breaks
// code that was written prior to C++20 being standardized and assumes providing
// .data() and .size() is sufficient.
// TODO: https://crbug.com/1504998 - Remove in favor of CompatibleRange and fix
// callsites.
template <typename T, typename R>
concept LegacyCompatibleRange = requires(R& r) {
  { *std::ranges::data(r) } -> LegalDataConversion<T>;
  std::ranges::size(r);
};

template <size_t I>
using size_constant = std::integral_constant<size_t, I>;

template <typename T>
struct ExtentImpl : size_constant<dynamic_extent> {};

template <typename T, size_t N>
struct ExtentImpl<T[N]> : size_constant<N> {};

template <typename T, size_t N>
struct ExtentImpl<std::array<T, N>> : size_constant<N> {};

template <typename T, size_t N>
struct ExtentImpl<base::span<T, N>> : size_constant<N> {};

template <typename T>
using Extent = ExtentImpl<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr size_t ExtentV = Extent<T>::value;

// must_not_be_dynamic_extent prevents |dynamic_extent| from being returned in a
// constexpr context.
template <size_t kExtent>
constexpr size_t must_not_be_dynamic_extent() {
  static_assert(
      kExtent != dynamic_extent,
      "EXTENT should only be used for containers with a static extent.");
  return kExtent;
}

template <class T, class U, size_t N, size_t M>
  requires((N == M || N == dynamic_extent || M == dynamic_extent) &&
           std::equality_comparable_with<T, U>)
constexpr bool span_cmp(span<T, N> l, span<U, M> r);

}  // namespace internal

// A span is a value type that represents an array of elements of type T. Since
// it only consists of a pointer to memory with an associated size, it is very
// light-weight. It is cheap to construct, copy, move and use spans, so that
// users are encouraged to use it as a pass-by-value parameter. A span does not
// own the underlying memory, so care must be taken to ensure that a span does
// not outlive the backing store.
//
// span is somewhat analogous to std::string_view, but with arbitrary element
// types, allowing mutation if T is non-const.
//
// span is implicitly convertible from C++ arrays, as well as most [1]
// container-like types that provide a data() and size() method (such as
// std::vector<T>). A mutable span<T> can also be implicitly converted to an
// immutable span<const T>.
//
// Consider using a span for functions that take a data pointer and size
// parameter: it allows the function to still act on an array-like type, while
// allowing the caller code to be a bit more concise.
//
// For read-only data access pass a span<const T>: the caller can supply either
// a span<const T> or a span<T>, while the callee will have a read-only view.
// For read-write access a mutable span<T> is required.
//
// Without span:
//   Read-Only:
//     // std::string HexEncode(const uint8_t* data, size_t size);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer.data(), data_buffer.size());
//
//  Mutable:
//     // ssize_t SafeSNPrintf(char* buf, size_t N, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, sizeof(str_buffer), "Pi ~= %lf", 3.14);
//
// With span:
//   Read-Only:
//     // std::string HexEncode(base::span<const uint8_t> data);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer);
//
//  Mutable:
//     // ssize_t SafeSNPrintf(base::span<char>, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, "Pi ~= %lf", 3.14);
//
// Dynamic vs Fixed size spans
// ---------------------------
//
// Normally spans have a dynamic size, which is represented as a type as
// `span<T>`. However it is possible to encode the size of the span into the
// type as a second parameter such as `span<T, N>`. When working with fixed-size
// spans, the compiler will check the size of operations and prevent compilation
// when an invalid size is used for an operation such as assignment or
// `copy_from()`. However operations that produce a new span will make a
// dynamic-sized span by default. See below for how to prevent that.
//
// Fixed-size spans implicitly convert to a dynamic-size span, throwing away the
// compile-time size information from the type signature. So most code should
// work with dynamic-sized `span<T>` types and not worry about the existence of
// fixed-size spans.
//
// It is possible to convert from a dynamic-size to a fixed-size span (or to
// move from a fixed-size span to another fixed-size span) but it requires
// writing an the size explicitly in the code. Methods like `first` can be
// passed a size as a template argument, such as `first<N>()` to generate a
// fixed-size span. And the `make_span` function can be given a compile-time
// size in a similar way with `make_span<N>()`.
//
// Spans with "const" and pointers
// -------------------------------
//
// Const and pointers can get confusing. Here are vectors of pointers and their
// corresponding spans:
//
//   const std::vector<int*>        =>  base::span<int* const>
//   std::vector<const int*>        =>  base::span<const int*>
//   const std::vector<const int*>  =>  base::span<const int* const>
//
// Differences from the C++ standard
// ---------------------------------
//
// http://eel.is/c++draft/views.span contains the latest C++ draft of std::span.
// Chromium tries to follow the draft as close as possible. Differences between
// the draft and the implementation are documented in subsections below.
//
// Differences from [span.overview]:
// - Dynamic spans are implemented as a partial specialization of the regular
//   class template. This leads to significantly simpler checks involving the
//   extent, at the expense of some duplicated code. The same strategy is used
//   by libc++.
//
// Differences from [span.objectrep]:
// - as_bytes() and as_writable_bytes() return spans of uint8_t instead of
//   std::byte.
//
// Differences from [span.cons]:
// - The constructors from a contiguous range apart from a C array are folded
//   into a single one, using a construct similarly to the one proposed
//   (but not standardized) in https://wg21.link/P1419.
//   The C array constructor is kept so that a span can be constructed from
//   an init list like {{1, 2, 3}}.
//   TODO: https://crbug.com/828324 - Consider adding C++26's constructor from
//   a std::initializer_list instead.
// - The conversion constructors from a contiguous range into a dynamic span
//   don't check for the range concept, but rather whether std::ranges::data
//   and std::ranges::size are well formed. This is due to legacy reasons and
//   should be fixed.
//
// Differences from [span.deduct]:
// - The deduction guides from a contiguous range are folded into a single one,
//   and treat borrowed ranges correctly.
// - Add deduction guide from rvalue array.
//
// Other differences:
// - Using StrictNumeric<size_t> instead of size_t where possible.
//
// Additions beyond the C++ standard draft
// - as_chars() function.
// - as_writable_chars() function.
// - as_byte_span() function.
// - as_writable_byte_span() function.
// - copy_from() method.
// - span_from_ref() function.
// - byte_span_from_ref() function.
// - span_from_cstring() function.
// - byte_span_from_cstring() function.
// - split_at() method.
// - operator==() comparator function.
//
// Furthermore, all constructors and methods are marked noexcept due to the lack
// of exceptions in Chromium.
//
// Due to the lack of class template argument deduction guides in C++14
// appropriate make_span() utility functions are provided for historic reasons.

// [span], class template span
template <typename T, size_t N, typename InternalPtrType>
class GSL_POINTER span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = CheckedContiguousIterator<T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_t extent = N;

  // [span.cons], span constructors, copy, assignment, and destructor
  constexpr span() noexcept
    requires(N == 0)
  = default;

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Checks
  // The function CHECKs that `count` matches the template parameter `N` and
  // will terminate otherwise.
  //
  // # Safety
  // The iterator must point to the first of at least `count` many elements, or
  // Undefined Behaviour can result as the span will allow access beyond the
  // valid range of the collection pointed to by the iterator.
  template <typename It>
    requires(internal::CompatibleIter<T, It>)
  UNSAFE_BUFFER_USAGE explicit constexpr span(
      It first,
      StrictNumeric<size_t> count) noexcept
      :  // The use of to_address() here is to handle the case where the
         // iterator `first` is pointing to the container's `end()`. In that
         // case we can not use the address returned from the iterator, or
         // dereference it through the iterator's `operator*`, but we can store
         // it. We must assume in this case that `count` is 0, since the
         // iterator does not point to valid data. Future hardening of iterators
         // may disallow pulling the address from `end()`, as demonstrated by
         // asserts() in libstdc++:
         // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93960.
         //
         // The span API dictates that the `data()` is accessible when size is
         // 0, since the pointer may be valid, so we cannot prevent storing and
         // giving out an invalid pointer here without breaking API
         // compatibility and our unit tests. Thus protecting against this can
         // likely only be successful from inside iterators themselves, where
         // the context about the pointer is known.
         //
         // We can not protect here generally against an invalid iterator/count
         // being passed in, since we have no context to determine if the
         // iterator or count are valid.
        data_(base::to_address(first)) {
    // Guarantees that the N in the type signature is correct.
    CHECK(N == count);
  }

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Checks
  // The function CHECKs that `it <= end` and will terminate otherwise.
  //
  // # Safety
  // The begin and end iterators must be for the same allocation or Undefined
  // Behaviour can result as the span will allow access beyond the valid range
  // of the collection pointed to by `begin`.
  template <typename It, typename End>
    requires(internal::CompatibleIter<T, It> &&
             std::sized_sentinel_for<End, It> &&
             !std::convertible_to<End, size_t>)
  UNSAFE_BUFFER_USAGE explicit constexpr span(It begin, End end) noexcept
      // SAFETY: The caller must guarantee that the iterator and end sentinel
      // are part of the same allocation, in which case it is the number of
      // elements between the iterators and thus a valid size for the pointer to
      // the element at `begin`.
      //
      // We CHECK that `end - begin` did not underflow below. Normally checking
      // correctness afterward is flawed, however underflow is not UB and the
      // size is not converted to an invalid pointer (which would be UB) before
      // we CHECK for underflow.
      : UNSAFE_BUFFERS(span(begin, static_cast<size_t>(end - begin))) {
    // Verify `end - begin` did not underflow.
    CHECK(begin <= end);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(T (&arr)[N]) noexcept
      // SAFETY: The std::ranges::size() function gives the number of elements
      // pointed to by the std::ranges::data() function, which meets the
      // requirement of span.
      : UNSAFE_BUFFERS(span(std::ranges::data(arr), std::ranges::size(arr))) {}

  template <typename R, size_t X = internal::ExtentV<R>>
    requires(internal::CompatibleRange<T, R> && (X == N || X == dynamic_extent))
  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit(X == dynamic_extent) constexpr span(R&& range) noexcept
      // SAFETY: The std::ranges::size() function gives the number of elements
      // pointed to by the std::ranges::data() function, which meets the
      // requirement of span.
      : UNSAFE_BUFFERS(
            span(std::ranges::data(range), std::ranges::size(range))) {}

  // [span.sub], span subviews
  template <size_t Count>
  constexpr span<T, Count> first() const noexcept
    requires(Count <= N)
  {
    // SAFETY: span provides that data() points to at least `N` many elements.
    // `Count` is non-negative by its type and `Count <= N` from the requires
    // condition. So `Count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS(span<T, Count>(data(), Count));
  }

  template <size_t Count>
  constexpr span<T, Count> last() const noexcept
    requires(Count <= N)
  {
    // SAFETY: span provides that data() points to at least `N` many elements.
    // `Count` is non-negative by its type and `Count <= N` from the requires
    // condition. So `0 <= N - Count <= N`, meaning `N - Count` is a valid new
    // size for `data()` and it will point to `Count` many elements.`
    return UNSAFE_BUFFERS(span<T, Count>(data() + (N - Count), Count));
  }

  // Returns a span over the first `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr span<T> first(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    // SAFETY: span provides that data() points to at least `N` many elements.
    // `count` is non-negative by its type and `count <= N` from the CHECK
    // above. So `count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS({data(), count});
  }

  // Returns a span over the last `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr span<T> last(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, N);
    // SAFETY: span provides that data() points to at least `N` many elements.
    // `count` is non-negative by its type and `count <= N` from the CHECK
    // above. So `0 <= N - count <= N`, meaning `N - count` is a valid new size
    // for `data()` and it will point to `count` many elements.
    return UNSAFE_BUFFERS({data() + (N - size_t{count}), count});
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr auto subspan() const noexcept
    requires(Offset <= N && (Count == dynamic_extent || Count <= N - Offset))
  {
    constexpr size_t kExtent = Count != dynamic_extent ? Count : N - Offset;
    // SAFETY: span provides that data() points to at least `N` many elements.
    //
    // If Count is dynamic_extent, kExtent becomes `N - Offset`. Since `Offset
    // <= N` from the requires condition, then `Offset` is a valid offset for
    // data(), and `Offset + kExtent = Offset + N - Offset = N >= Offset` is
    // also a valid offset that is not before `Offset`. This makes a span at
    // `Offset` with size `kExtent` valid.
    //
    // Otherwise `Count <= N - Offset` and `0 <= Offset <= N` by the requires
    // condition, so `Offset <= N - Count` and `N - Count` can not underflow.
    // Then `Offset` is a valid offset for data() and `kExtent` is `Count <= N -
    // Offset`, so `Offset + kExtent <= Offset + N - Offset = N` which makes
    // both `Offset` and `Offset + kExtent` valid offsets for data(), and since
    // `kExtent` is non-negative, `Offset + kExtent` is not before `Offset` so
    // `kExtent` is a valid size for the span at `data() + Offset`.
    return UNSAFE_BUFFERS(span<T, kExtent>(data() + Offset, kExtent));
  }

  // Returns a span over the first `count` elements starting at the given
  // `offset` from the start of the span.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset + count`
  // elements, or at least `offset` elements if `count` is not specified, and
  // will terminate otherwise.
  constexpr span<T> subspan(size_t offset,
                            size_t count = dynamic_extent) const noexcept {
    CHECK_LE(offset, N);
    CHECK(count == dynamic_extent || count <= N - offset);
    const size_t new_extent = count != dynamic_extent ? count : N - offset;
    // SAFETY: span provides that data() points to at least `N` many elements.
    //
    // If Count is dynamic_extent, `new_extent` becomes `N - offset`. Since
    // `offset <= N` from the requires condition, then `offset` is a valid
    // offset for data(), and `offset + new_extent = offset + N - offset = N >=
    // offset` is also a valid offset that is not before `offset`. This makes a
    // span at `offset` with size `new_extent` valid.
    //
    // Otherwise `count <= N - offset` and `0 <= offset <= N` by the requires
    // condition, so `offset <= N - count` and `N - count` can not underflow.
    // Then `offset` is a valid offset for data() and `new_extent` is `count <=
    // N - offset`, so `offset + new_extent <= offset + N - offset = N` which
    // makes both `offset` and `offset + new_extent` valid offsets for data(),
    // and since `new_extent` is non-negative, `offset + new_extent` is not
    // before `offset` so `new_extent` is a valid size for the span at `data() +
    // offset`.
    return UNSAFE_BUFFERS({data() + offset, new_extent});
  }

  // Splits a span into two at the given `offset`, returning two spans that
  // cover the full range of the original span.
  //
  // Similar to calling subspan() with the `offset` as the length on the first
  // call, and then the `offset` as the offset in the second.
  //
  // The split_at<N>() overload allows construction of a fixed-size span from a
  // compile-time constant. If the input span is fixed-size, both output output
  // spans will be. Otherwise, the first will be fixed-size and the second will
  // be dynamic-size.
  //
  // This is a non-std extension that  is inspired by the Rust slice::split_at()
  // and split_at_mut() methods.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset` elements and
  // will terminate otherwise.
  constexpr std::pair<span<T>, span<T>> split_at(size_t offset) const noexcept {
    return {first(offset), subspan(offset)};
  }

  template <size_t Offset>
    requires(Offset <= N)
  constexpr std::pair<span<T, Offset>, span<T, N - Offset>> split_at()
      const noexcept {
    return {first<Offset>(), subspan<Offset, N - Offset>()};
  }

  // [span.obs], span observers
  constexpr size_t size() const noexcept { return N; }
  constexpr size_t size_bytes() const noexcept { return size() * sizeof(T); }
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // [span.elem], span element access
  //
  // # Checks
  // The function CHECKs that the `idx` is inside the span and will terminate
  // otherwise.
  constexpr T& operator[](size_t idx) const noexcept {
    CHECK_LT(idx, size());
    // SAFETY: Since data() always points to at least `N` elements, the check
    // above ensures `idx < N` and is thus in range for data().
    return UNSAFE_BUFFERS(data()[idx]);
  }

  constexpr T& front() const noexcept
    requires(N > 0)
  {
    // SAFETY: Since data() always points to at least `N` elements, the requires
    // constraint above ensures `0 < N` and is thus in range for data().
    return UNSAFE_BUFFERS(data()[0]);
  }

  constexpr T& back() const noexcept
    requires(N > 0)
  {
    // SAFETY: Since data() always points to at least `N` elements, the requires
    // constraint above ensures `N > 0` and thus `N - 1` does not underflow and
    // is in range for data().
    return UNSAFE_BUFFERS(data()[N - 1]);
  }

  // Returns a pointer to the first element in the span. If the span is empty
  // (`size()` is 0), the returned pointer may or may not be null, and it must
  // not be dereferenced.
  //
  // It is always valid to add `size()` to the the pointer in C++ code, though
  // it may be invalid in C code when the span is empty.
  constexpr T* data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept {
    // SAFETY: span provides that data() points to at least `size()` many
    // elements, and size() is non-negative. So data() + size() is a valid
    // pointer for the data() allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + size()));
  }

  constexpr iterator end() const noexcept {
    // SAFETY: span provides that data() points to at least `size()` many
    // elements, and size() is non-negative. So data() + size() is a valid
    // pointer for the data() allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + size(), data() + size()));
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  // Bounds-checked copy from a non-overlapping span. The spans must be the
  // exact same size or a hard CHECK() occurs. If the two spans overlap,
  // Undefined Behaviour occurs.
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  //
  // # Checks
  // The function CHECKs that the `other` span has the same size as itself and
  // will terminate otherwise.
  constexpr void copy_from(span<const T, N> other)
    requires(!std::is_const_v<T>)
  {
    CHECK_EQ(size_bytes(), other.size_bytes());
    // Verify non-overlapping in developer builds.
    //
    // SAFETY: span provides that data() points to at least size() many
    // elements, so adding size() to the data() pointer is well-defined.
    DCHECK(UNSAFE_BUFFERS(data() + size()) <= other.data() ||
           data() >= UNSAFE_BUFFERS(other.data() + other.size()));
    // When compiling with -Oz, std::ranges::copy() does not get inlined, which
    // makes copy_from() very expensive compared to memcpy for small sizes (up
    // to around 4x slower). We observe that this is because ranges::copy() uses
    // begin()/end() and span's iterators are checked iterators, not just
    // pointers. This additional complexity prevents inlining and breaks the
    // ability for the compiler to eliminate code.
    //
    // See also https://crbug.com/1396134.
    //
    // We also see std::copy() (with pointer arguments! not iterators) optimize
    // and inline better than memcpy() since memcpy() needs to rely on
    // size_bytes(), which while computable at compile time when `other` has a
    // fixed size, the optimizer stumbles on with -Oz.
    //
    // SAFETY: The copy() here does not check bounds, but we have verified that
    // `this` and `other` have the same bounds above (and are pointers of the
    // same type), so `data()` and `other.data()` both have at least
    // `other.size()` elements.
    UNSAFE_BUFFERS(
        std::copy(other.data(), other.data() + other.size(), data()));
  }

  // Implicit conversion from std::span<T, N> to base::span<T, N>.
  //
  // We get other conversions for free from std::span's constructors, but it
  // does not deduce N on its range constructor.
  span(std::span<std::remove_const_t<T>, N> other)
      :  // SAFETY: std::span contains a valid data pointer and size such
         // that pointer+size remains valid.
        UNSAFE_BUFFERS(
            span(std::ranges::data(other), std::ranges::size(other))) {}
  span(std::span<T, N> other)
    requires(std::is_const_v<T>)
      :  // SAFETY: std::span contains a valid data pointer and size such
         // that pointer+size remains valid.
        UNSAFE_BUFFERS(
            span(std::ranges::data(other), std::ranges::size(other))) {}

  // Implicit conversion from base::span<T, N> to std::span<T, N>.
  //
  // We get other conversions for free from std::span's constructors, but it
  // does not deduce N on its range constructor.
  operator std::span<T, N>() const { return std::span<T, N>(*this); }
  operator std::span<const T, N>() const
    requires(!std::is_const_v<T>)
  {
    return std::span<const T, N>(*this);
  }

  // Compares two spans for equality by comparing the objects pointed to by the
  // spans. The operation is defined for spans of different types as long as the
  // types are themselves comparable.
  //
  // For primitive types, this replaces the less safe `memcmp` function, where
  // `memcmp(a.data(), b.data(), a.size())` can be written as `a == b` and can
  // no longer go outside the bounds of `b`. Otherwise, it replaced std::equal
  // or std::ranges::equal when working with spans, and when no projection is
  // needed.
  //
  // If the spans are of different sizes, they are not equal. If both spans are
  // empty, they are always equal (even though their data pointers may differ).
  //
  // # Implementation note
  // The non-template overloads allow implicit conversions to span for
  // comparison.
  friend constexpr bool operator==(span lhs, span rhs)
    requires(std::equality_comparable<const T>)
  {
    return internal::span_cmp(span<const T, N>(lhs), span<const T, N>(rhs));
  }
  friend constexpr bool operator==(span lhs, span<const T, N> rhs)
    requires(!std::is_const_v<T> && std::equality_comparable<const T>)
  {
    return internal::span_cmp(span<const T, N>(lhs), span<const T, N>(rhs));
  }
  template <class U, size_t M>
    requires((N == M || M == dynamic_extent) &&
             std::equality_comparable_with<const T, const U>)
  friend constexpr bool operator==(span lhs, span<U, M> rhs) {
    return internal::span_cmp(span<const T, N>(lhs), span<const U, M>(rhs));
  }

 private:
  // This field is not a raw_ptr<> since span is mostly used for stack
  // variables. Use `raw_span` instead for class fields, which does use
  // raw_ptr<> internally.
  InternalPtrType data_ = nullptr;
};

// [span], class template span
template <typename T, typename InternalPtrType>
class GSL_POINTER span<T, dynamic_extent, InternalPtrType> {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = CheckedContiguousIterator<T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_t extent = dynamic_extent;

  constexpr span() noexcept = default;

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Safety
  // The iterator must point to the first of at least `count` many elements, or
  // Undefined Behaviour can result as the span will allow access beyond the
  // valid range of the collection pointed to by the iterator.
  template <typename It>
    requires(internal::CompatibleIter<T, It>)
  UNSAFE_BUFFER_USAGE constexpr span(It first,
                                     StrictNumeric<size_t> count) noexcept
      // The use of to_address() here is to handle the case where the iterator
      // `first` is pointing to the container's `end()`. In that case we can
      // not use the address returned from the iterator, or dereference it
      // through the iterator's `operator*`, but we can store it. We must
      // assume in this case that `count` is 0, since the iterator does not
      // point to valid data. Future hardening of iterators may disallow
      // pulling the address from `end()`, as demonstrated by asserts() in
      // libstdc++: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93960.
      //
      // The span API dictates that the `data()` is accessible when size is 0,
      // since the pointer may be valid, so we cannot prevent storing and
      // giving out an invalid pointer here without breaking API compatibility
      // and our unit tests. Thus protecting against this can likely only be
      // successful from inside iterators themselves, where the context about
      // the pointer is known.
      //
      // We can not protect here generally against an invalid iterator/count
      // being passed in, since we have no context to determine if the
      // iterator or count are valid.
      : data_(base::to_address(first)), size_(count) {}

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Safety
  // The begin and end iterators must be for the same allocation, and `begin <=
  // end` or Undefined Behaviour can result as the span will allow access beyond
  // the valid range of the collection pointed to by `begin`.
  template <typename It, typename End>
    requires(internal::CompatibleIter<T, It> &&
             std::sized_sentinel_for<End, It> &&
             !std::convertible_to<End, size_t>)
  UNSAFE_BUFFER_USAGE constexpr span(It begin, End end) noexcept
      // SAFETY: The caller must guarantee that the iterator and end sentinel
      // are part of the same allocation, in which case it is the number of
      // elements between the iterators and thus a valid size for the pointer to
      // the element at `begin`.
      //
      // We CHECK that `end - begin` did not underflow below. Normally checking
      // correctness afterward is flawed, however underflow is not UB and the
      // size is not converted to an invalid pointer (which would be UB) before
      // we CHECK for underflow.
      : UNSAFE_BUFFERS(span(begin, static_cast<size_t>(end - begin))) {
    // Verify `end - begin` did not underflow.
    CHECK(begin <= end);
  }

  template <size_t N>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(T (&arr)[N]) noexcept
      // SAFETY: The std::ranges::size() function gives the number of elements
      // pointed to by the std::ranges::data() function, which meets the
      // requirement of span.
      : UNSAFE_BUFFERS(span(std::ranges::data(arr), std::ranges::size(arr))) {}

  template <typename R>
    requires(internal::LegacyCompatibleRange<T, R>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(R&& range) noexcept
      // SAFETY: The std::ranges::size() function gives the number of elements
      // pointed to by the std::ranges::data() function, which meets the
      // requirement of span.
      : UNSAFE_BUFFERS(
            span(std::ranges::data(range), std::ranges::size(range))) {}

  // [span.sub], span subviews
  template <size_t Count>
  constexpr span<T, Count> first() const noexcept {
    CHECK_LE(Count, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `Count` is non-negative by its type and `Count <= size()` from
    // the CHECK above. So `Count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS(span<T, Count>(data(), Count));
  }

  template <size_t Count>
  constexpr span<T, Count> last() const noexcept {
    CHECK_LE(Count, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `Count` is non-negative by its type and `Count <= size()` from
    // the check above. So `0 <= size() - Count <= size()`, meaning
    // `size() - Count` is a valid new size for `data()` and it will point to
    // `Count` many elements.
    return UNSAFE_BUFFERS(span<T, Count>(data() + (size() - Count), Count));
  }

  // Returns a span over the first `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr span<T> first(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `count` is non-negative by its type and `count <= size()` from
    // the CHECK above. So `count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS({data(), count});
  }

  // Returns a span over the last `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr span<T> last(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `count` is non-negative by its type and `count <= size()` from
    // the CHECK above. So `0 <= size() - count <= size()`, meaning
    // `size() - count` is a valid new size for `data()` and it will point to
    // `count` many elements.
    return UNSAFE_BUFFERS({data() + (size() - size_t{count}), count});
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr span<T, Count> subspan() const noexcept {
    CHECK_LE(Offset, size());
    CHECK(Count == dynamic_extent || Count <= size() - Offset);
    const size_t new_extent = Count != dynamic_extent ? Count : size() - Offset;
    // SAFETY: span provides that data() points to at least `size()` many
    // elements.
    //
    // If Count is dynamic_extent, `new_extent` becomes `size() - Offset`. Since
    // `Offset <= size()` from the check above, then `Offset` is a valid offset
    // for data(), and `Offset + new_extent = Offset + size() - Offset = size()
    // >= Offset` is also a valid offset that is not before `Offset`. This makes
    // a span at `Offset` with size `new_extent` valid.
    //
    // Otherwise `Count <= size() - Offset` and `0 <= Offset <= size()` by the
    // check above, so `Offset <= size() - Count` and `size() - Count` can not
    // underflow. Then `Offset` is a valid offset for data() and `new_extent` is
    // `Count <= size() - Offset`, so `Offset + extent <= Offset + size() -
    // Offset = size()` which makes both `Offset` and `Offset + new_extent`
    // valid offsets for data(), and since `new_extent` is non-negative, `Offset
    // + new_extent` is not before `Offset` so `new_extent` is a valid size for
    // the span at `data() + Offset`.
    return UNSAFE_BUFFERS(span<T, Count>(data() + Offset, new_extent));
  }

  // Returns a span over the first `count` elements starting at the given
  // `offset` from the start of the span.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset + count`
  // elements, or at least `offset` elements if `count` is not specified, and
  // will terminate otherwise.
  constexpr span<T> subspan(size_t offset,
                            size_t count = dynamic_extent) const noexcept {
    CHECK_LE(offset, size());
    CHECK(count == dynamic_extent || count <= size() - offset);
    const size_t new_extent = count != dynamic_extent ? count : size() - offset;
    // SAFETY: span provides that data() points to at least `size()` many
    // elements.
    //
    // If count is dynamic_extent, `new_extent` becomes `size() - offset`. Since
    // `offset <= size()` from the check above, then `offset` is a valid offset
    // for data(), and `offset + new_extent = offset + size() - offset = size()
    // >= offset` is also a valid offset that is not before `offset`. This makes
    // a span at `offset` with size `new_extent` valid.
    //
    // Otherwise `count <= size() - offset` and `0 <= offset <= size()` by the
    // checks above, so `offset <= size() - count` and `size() - count` can not
    // underflow. Then `offset` is a valid offset for data() and `new_extent` is
    // `count <= size() - offset`, so `offset + new_extent <= offset + size() -
    // offset = size()` which makes both `offset` and `offset + new_extent`
    // valid offsets for data(), and since `new_extent` is non-negative, `offset
    // + new_extent` is not before `offset` so `new_extent` is a valid size for
    // the span at `data() + offset`.
    return UNSAFE_BUFFERS({data() + offset, new_extent});
  }

  // Splits a span into two at the given `offset`, returning two spans that
  // cover the full range of the original span.
  //
  // Similar to calling subspan() with the `offset` as the length on the first
  // call, and then the `offset` as the offset in the second.
  //
  // The split_at<N>() overload allows construction of a fixed-size span from a
  // compile-time constant. If the input span is fixed-size, both output output
  // spans will be. Otherwise, the first will be fixed-size and the second will
  // be dynamic-size.
  //
  // This is a non-std extension that  is inspired by the Rust slice::split_at()
  // and split_at_mut() methods.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset` elements and
  // will terminate otherwise.
  constexpr std::pair<span<T>, span<T>> split_at(size_t offset) const noexcept {
    return {first(offset), subspan(offset)};
  }

  // An overload of `split_at` which returns a fixed-size span.
  //
  // # Checks
  // The function CHECKs that the span contains at least `Offset` elements and
  // will terminate otherwise.
  template <size_t Offset>
  constexpr std::pair<span<T, Offset>, span<T>> split_at() const noexcept {
    CHECK_LE(Offset, size());
    return {first<Offset>(), subspan(Offset)};
  }

  // [span.obs], span observers
  constexpr size_t size() const noexcept { return size_; }
  constexpr size_t size_bytes() const noexcept { return size() * sizeof(T); }
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // [span.elem], span element access
  //
  // # Checks
  // The function CHECKs that the `idx` is inside the span and will terminate
  // otherwise.
  constexpr T& operator[](size_t idx) const noexcept {
    CHECK_LT(idx, size());
    // SAFETY: Since data() always points to at least `size()` elements, the
    // check above ensures `idx < size()` and is thus in range for data().
    return UNSAFE_BUFFERS(data()[idx]);
  }

  // Returns a reference to the first element in the span.
  //
  // # Checks
  // The function CHECKs that the span is not empty and will terminate
  // otherwise.
  constexpr T& front() const noexcept {
    CHECK(!empty());
    // SAFETY: Since data() always points to at least `size()` elements, the
    // check above above ensures `0 < size()` and is thus in range for data().
    return UNSAFE_BUFFERS(data()[0]);
  }

  // Returns a reference to the last element in the span.
  //
  // # Checks
  // The function CHECKs that the span is not empty and will terminate
  // otherwise.
  constexpr T& back() const noexcept {
    CHECK(!empty());
    // SAFETY: Since data() always points to at least `size()` elements, the
    // check above above ensures `size() > 0` and thus `size() - 1` does not
    // underflow and is in range for data().
    return UNSAFE_BUFFERS(data()[size() - 1]);
  }

  // Returns a pointer to the first element in the span. If the span is empty
  // (`size()` is 0), the returned pointer may or may not be null, and it must
  // not be dereferenced.
  //
  // It is always valid to add `size()` to the the pointer in C++ code, though
  // it may be invalid in C code when the span is empty.
  constexpr T* data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept {
    // SAFETY: span provides that data() points to at least `size()` many
    // elements, and size() is non-negative. So data() + size() is a valid
    // pointer for the data() allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + size()));
  }

  constexpr iterator end() const noexcept {
    // SAFETY: span provides that data() points to at least `size()` many
    // elements, and size() is non-negative. So data() + size() is a valid
    // pointer for the data() allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + size(), data() + size()));
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  // Bounds-checked copy from a non-overlapping span. The spans must be the
  // exact same size or a hard CHECK() occurs. If the two spans overlap,
  // Undefined Behaviour occurs.
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  //
  // # Checks
  // The function CHECKs that the `other` span has the same size as itself and
  // will terminate otherwise.
  constexpr void copy_from(span<const T> other)
    requires(!std::is_const_v<T>)
  {
    CHECK_EQ(size_bytes(), other.size_bytes());
    // Verify non-overlapping in developer builds.
    //
    // SAFETY: span provides that data() points to at least size() many
    // elements, so adding size() to the data() pointer is well-defined.
    DCHECK(UNSAFE_BUFFERS(data() + size()) <= other.data() ||
           data() >= UNSAFE_BUFFERS(other.data() + other.size()));
    // When compiling with -Oz, std::ranges::copy() does not get inlined, which
    // makes copy_from() very expensive compared to memcpy for small sizes (up
    // to around 4x slower). We observe that this is because ranges::copy() uses
    // begin()/end() and span's iterators are checked iterators, not just
    // pointers. This additional complexity prevents inlining and breaks the
    // ability for the compiler to eliminate code.
    //
    // See also https://crbug.com/1396134.
    //
    // We also see std::copy() (with pointer arguments! not iterators) optimize
    // and inline better than memcpy() since memcpy() needs to rely on
    // size_bytes(), which while computable at compile time when `other` has a
    // fixed size, the optimizer stumbles on with -Oz.
    //
    // SAFETY: The copy() here does not check bounds, but we have verified that
    // `this` and `other` have the same bounds above (and are pointers of the
    // same type), so `data()` and `other.data()` both have at least
    // `other.size()` elements.
    UNSAFE_BUFFERS(
        std::copy(other.data(), other.data() + other.size(), data()));
  }

  // Compares two spans for equality by comparing the objects pointed to by the
  // spans. The operation is defined for spans of different types as long as the
  // types are themselves comparable.
  //
  // For primitive types, this replaces the less safe `memcmp` function, where
  // `memcmp(a.data(), b.data(), a.size())` can be written as `a == b` and can
  // no longer go outside the bounds of `b`. Otherwise, it replaced std::equal
  // or std::ranges::equal when working with spans, and when no projection is
  // needed.
  //
  // If the spans are of different sizes, they are not equal. If both spans are
  // empty, they are always equal (even though their data pointers may differ).
  //
  // # Implementation note
  // The non-template overloads allow implicit conversions to span for
  // comparison.
  friend constexpr bool operator==(span lhs, span rhs)
    requires(std::equality_comparable<const T>)
  {
    return internal::span_cmp(span<const T>(lhs), span<const T>(rhs));
  }
  friend constexpr bool operator==(span lhs, span<const T> rhs)
    requires(!std::is_const_v<T> && std::equality_comparable<const T>)
  {
    return internal::span_cmp(span<const T>(lhs), span<const T>(rhs));
  }
  template <class U, size_t M>
    requires(std::equality_comparable_with<const T, const U>)
  friend constexpr bool operator==(span lhs, span<U, M> rhs) {
    return internal::span_cmp(span<const T>(lhs), span<const U, M>(rhs));
  }

 private:
  // This field is not a raw_ptr<> since span is mostly used for stack
  // variables. Use `raw_span` instead for class fields, which does use
  // raw_ptr<> internally.
  InternalPtrType data_ = nullptr;
  size_t size_ = 0;
};

// [span.deduct], deduction guides.
template <typename It, typename EndOrSize>
  requires(std::contiguous_iterator<It>)
span(It, EndOrSize) -> span<std::remove_reference_t<std::iter_reference_t<It>>>;

template <
    typename R,
    typename T = std::remove_reference_t<std::ranges::range_reference_t<R>>>
  requires(std::ranges::contiguous_range<R>)
span(R&&)
    -> span<std::conditional_t<std::ranges::borrowed_range<R>, T, const T>,
            internal::ExtentV<R>>;

template <typename T, size_t N>
span(const T (&)[N]) -> span<const T, N>;

// [span.objectrep], views of object representation
template <typename T, size_t X>
constexpr auto as_bytes(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  // SAFETY: span provides that data() points to at least size_bytes() many
  // bytes. So since `uint8_t` has a size of 1 byte, the size_bytes() value is
  // a valid size for a span at data() when viewed as `uint8_t*`.
  //
  // The reinterpret_cast is valid as the alignment of uint8_t (which is 1) is
  // always less-than or equal to the alignment of T.
  return UNSAFE_BUFFERS(span<const uint8_t, N>(
      reinterpret_cast<const uint8_t*>(s.data()), s.size_bytes()));
}

template <typename T, size_t X>
  requires(!std::is_const_v<T>)
constexpr auto as_writable_bytes(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  // SAFETY: span provides that data() points to at least size_bytes() many
  // bytes. So since `uint8_t` has a size of 1 byte, the size_bytes() value is a
  // valid size for a span at data() when viewed as `uint8_t*`.
  //
  // The reinterpret_cast is valid as the alignment of uint8_t (which is 1) is
  // always less-than or equal to the alignment of T.
  return UNSAFE_BUFFERS(
      span<uint8_t, N>(reinterpret_cast<uint8_t*>(s.data()), s.size_bytes()));
}

// as_chars() is the equivalent of as_bytes(), except that it returns a
// span of const char rather than const uint8_t. This non-std function is
// added since chrome still represents many things as char arrays which
// rightfully should be uint8_t.
template <typename T, size_t X>
constexpr auto as_chars(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  // SAFETY: span provides that data() points to at least size_bytes() many
  // bytes. So since `char` has a size of 1 byte, the size_bytes() value is a
  // valid size for a span at data() when viewed as `char*`.
  //
  // The reinterpret_cast is valid as the alignment of char (which is 1) is
  // always less-than or equal to the alignment of T.
  return UNSAFE_BUFFERS(span<const char, N>(
      reinterpret_cast<const char*>(s.data()), s.size_bytes()));
}

// as_string_view() converts a span over byte-sized primitives (holding chars or
// uint8_t) into a std::string_view, where each byte is represented as a char.
// It also accepts any type that can implicitly convert to a span, such as
// arrays.
//
// If you want to view an arbitrary span type as a string, first explicitly
// convert it to bytes via `base::as_bytes()`.
//
// For spans over byte-sized primitives, this is sugar for:
// ```
// std::string_view(as_chars(span).begin(), as_chars(span).end())
// ```
constexpr std::string_view as_string_view(span<const char> s) noexcept {
  return std::string_view(s.begin(), s.end());
}
constexpr std::string_view as_string_view(
    span<const unsigned char> s) noexcept {
  const auto c = as_chars(s);
  return std::string_view(c.begin(), c.end());
}

// as_writable_chars() is the equivalent of as_writable_bytes(), except that
// it returns a span of char rather than uint8_t. This non-std function is
// added since chrome still represents many things as char arrays which
// rightfully should be uint8_t.
template <typename T, size_t X>
  requires(!std::is_const_v<T>)
auto as_writable_chars(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  // SAFETY: span provides that data() points to at least size_bytes() many
  // bytes. So since `char` has a size of 1 byte, the size_bytes() value is
  // a valid size for a span at data() when viewed as `char*`.
  //
  // The reinterpret_cast is valid as the alignment of char (which is 1) is
  // always less-than or equal to the alignment of T.
  return UNSAFE_BUFFERS(
      span<char, N>(reinterpret_cast<char*>(s.data()), s.size_bytes()));
}

// Type-deducing helper for constructing a span.
//
// # Safety
// The contiguous iterator `it` must point to the first element of at least
// `size` many elements or Undefined Behaviour may result as the span may give
// access beyond the bounds of the collection pointed to by `it`.
template <int&... ExplicitArgumentBarrier, typename It>
UNSAFE_BUFFER_USAGE constexpr auto make_span(
    It it,
    StrictNumeric<size_t> size) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  // SAFETY: The caller guarantees that `it` is the first of at least `size`
  // many elements.
  return UNSAFE_BUFFERS(span<T>(it, size));
}

// Type-deducing helper for constructing a span.
//
// # Checks
// The function CHECKs that `it <= end` and will terminate otherwise.
//
// # Safety
// The contiguous iterator `it` and its end sentinel `end` must be for the same
// allocation or Undefined Behaviour may result as the span may give access
// beyond the bounds of the collection pointed to by `it`.
template <int&... ExplicitArgumentBarrier,
          typename It,
          typename End,
          typename = std::enable_if_t<!std::is_convertible_v<End, size_t>>>
UNSAFE_BUFFER_USAGE constexpr auto make_span(It it, End end) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  // SAFETY: The caller guarantees that `it` and `end` are iterators of the
  // same allocation.
  return UNSAFE_BUFFERS(span<T>(it, end));
}

// make_span utility function that deduces both the span's value_type and extent
// from the passed in argument.
//
// Usage: auto span = base::make_span(...);
template <int&... ExplicitArgumentBarrier, typename Container>
constexpr auto make_span(Container&& container) noexcept {
  using T =
      std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>;
  using Extent = internal::Extent<Container>;
  return span<T, Extent::value>(std::forward<Container>(container));
}

// make_span utility function that allows callers to explicit specify the span's
// extent, the value_type is deduced automatically. This is useful when passing
// a dynamically sized container to a method expecting static spans, when the
// container is known to have the correct size.
//
// Note: This will CHECK that N indeed matches size(container).
//
// # Usage
// As this function is unsafe, the caller must guarantee that the size is
// correct for the iterator, and will not allow the span to reach out of bounds.
// ```
// // SAFETY: <An explanation of how the size is checked/ensured to always be
// // valid for the iterator>.
// auto static_span = UNSAFE_BUFFERS(base::make_span<N>(it, size));
// ```
//
// # Safety
// The contiguous iterator `it` must point to the first element of at least
// `size` many elements or Undefined Behaviour may result as the span may give
// access beyond the bounds of the collection pointed to by `it`.
template <size_t N, int&... ExplicitArgumentBarrier, typename It>
UNSAFE_BUFFER_USAGE constexpr auto make_span(
    It it,
    StrictNumeric<size_t> size) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  // SAFETY: The caller guarantees that `it` is the first of at least `size`
  // many elements.
  return UNSAFE_BUFFERS(span<T, N>(it, size));
}

// make_span utility function that allows callers to explicit specify the span's
// extent, the value_type is deduced automatically. This is useful when passing
// a dynamically sized container to a method expecting static spans, when the
// container is known to have the correct size.
//
// Note: This will CHECK that N indeed matches size(container).
//
// # Usage
// As this function is unsafe, the caller must guarantee that the `end` is from
// the same allocation as the `it` iterator.
// ```
// // SAFETY: <An explanation if non-trivial how the iterators are not from
// // different containers/allocations>.
// auto static_span = UNSAFE_BUFFERS(base::make_span<N>(it, end));
// ```
//
// # Checks
// The function CHECKs that `it <= end` and will terminate otherwise.
//
// # Safety
// The contiguous iterator `it` and its end sentinel `end` must be for the same
// allocation or Undefined Behaviour may result as the span may give access
// beyond the bounds of the collection pointed to by `it`.
template <size_t N,
          int&... ExplicitArgumentBarrier,
          typename It,
          typename End,
          typename = std::enable_if_t<!std::is_convertible_v<End, size_t>>>
UNSAFE_BUFFER_USAGE constexpr auto make_span(It it, End end) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  // SAFETY: The caller guarantees that `it` and `end` are iterators of the
  // same allocation.
  return UNSAFE_BUFFERS(span<T, N>(it, end));
}

template <size_t N, int&... ExplicitArgumentBarrier, typename Container>
constexpr auto make_span(Container&& container) noexcept {
  using T =
      std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>;
  // SAFETY: The std::size() function gives the number of elements pointed to by
  // the std::data() function, which meets the requirement of span.
  return UNSAFE_BUFFERS(span<T, N>(std::data(container), std::size(container)));
}

// `span_from_ref` converts a reference to T into a span of length 1.  This is a
// non-std helper that is inspired by the `std::slice::from_ref()` function from
// Rust.
template <typename T>
constexpr span<T, 1u> span_from_ref(
    T& single_object ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  // SAFETY: Given a valid reference to `single_object` the span of size 1 will
  // be a valid span that points to the `single_object`.
  return UNSAFE_BUFFERS(span<T, 1u>(std::addressof(single_object), 1u));
}

// `byte_span_from_ref` converts a reference to T into a span of uint8_t of
// length sizeof(T).  This is a non-std helper that is a sugar for
// `as_writable_bytes(span_from_ref(x))`.
//
// Const references are turned into a `span<const T, sizeof(T)>` while mutable
// references are turned into a `span<T, sizeof(T)>`.
template <typename T>
constexpr span<const uint8_t, sizeof(T)> byte_span_from_ref(
    const T& single_object ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return as_bytes(span_from_ref(single_object));
}
template <typename T>
constexpr span<uint8_t, sizeof(T)> byte_span_from_ref(
    T& single_object ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return as_writable_bytes(span_from_ref(single_object));
}

// Converts a string literal (such as `"hello"`) to a span of `char` while
// omitting the terminating NUL character. These two are equivalent:
// ```
// base::span<char, 5u> s1 = base::span_from_cstring("hello");
// base::span<char, 5u> s2 = base::span(std::string_view("hello"));
// ```
//
// If you want to include the NUL terminator, then use the span constructor
// directly, such as:
// ```
// base::span<char, 6u> s = base::span("hello");
// ```
template <size_t N>
constexpr span<const char, N - 1> span_from_cstring(
    const char (&lit ABSL_ATTRIBUTE_LIFETIME_BOUND)[N]) {
  return span(lit).template first<N - 1>();
}

// Converts a string literal (such as `"hello"`) to a span of `uint8_t` while
// omitting the terminating NUL character. These two are equivalent:
// ```
// base::span<uint8_t, 5u> s1 = base::byte_span_from_cstring("hello");
// base::span<uint8_t, 5u> s2 = base::as_byte_span(std::string_view("hello"));
// ```
//
// If you want to include the NUL terminator, then use the span constructor
// directly, such as:
// ```
// base::span<uint8_t, 6u> s = base::as_bytes(base::span("hello"));
// ```
template <size_t N>
constexpr span<const uint8_t, N - 1> byte_span_from_cstring(
    const char (&lit ABSL_ATTRIBUTE_LIFETIME_BOUND)[N]) {
  return as_bytes(span(lit).template first<N - 1>());
}

// Convenience function for converting an object which is itself convertible
// to span into a span of bytes (i.e. span of const uint8_t). Typically used
// to convert std::string or string-objects holding chars, or std::vector
// or vector-like objects holding other scalar types, prior to passing them
// into an API that requires byte spans.
template <typename T>
  requires requires(const T& arg) {
    requires !std::is_array_v<std::remove_reference_t<T>>;
    make_span(arg);
  }
constexpr span<const uint8_t> as_byte_span(const T& arg) {
  return as_bytes(make_span(arg));
}

// This overload for arrays preserves the compile-time size N of the array in
// the span type signature span<uint8_t, N>.
template <typename T, size_t N>
constexpr span<const uint8_t, N * sizeof(T)> as_byte_span(
    const T (&arr ABSL_ATTRIBUTE_LIFETIME_BOUND)[N]) {
  return as_bytes(make_span<N>(arr));
}

// Convenience function for converting an object which is itself convertible
// to span into a span of mutable bytes (i.e. span of uint8_t). Typically used
// to convert std::string or string-objects holding chars, or std::vector
// or vector-like objects holding other scalar types, prior to passing them
// into an API that requires mutable byte spans.
template <typename T>
  requires requires(T&& arg) {
    requires !std::is_array_v<std::remove_reference_t<T>>;
    make_span(arg);
    requires !std::is_const_v<typename decltype(make_span(arg))::element_type>;
  }
constexpr span<uint8_t> as_writable_byte_span(T&& arg) {
  return as_writable_bytes(make_span(arg));
}

// This overload for arrays preserves the compile-time size N of the array in
// the span type signature span<uint8_t, N>.
template <typename T, size_t N>
  requires(!std::is_const_v<T>)
constexpr span<uint8_t, N * sizeof(T)> as_writable_byte_span(
    T (&arr ABSL_ATTRIBUTE_LIFETIME_BOUND)[N]) {
  return as_writable_bytes(make_span<N>(arr));
}
template <typename T, size_t N>
  requires(!std::is_const_v<T>)
constexpr span<uint8_t, N * sizeof(T)> as_writable_byte_span(
    T (&&arr ABSL_ATTRIBUTE_LIFETIME_BOUND)[N]) {
  return as_writable_bytes(make_span<N>(arr));
}

namespace internal {

// Template helper for implementing operator==.
template <class T, class U, size_t N, size_t M>
  requires((N == M || N == dynamic_extent || M == dynamic_extent) &&
           std::equality_comparable_with<T, U>)
constexpr bool span_cmp(span<T, N> l, span<U, M> r) {
  return l.size() == r.size() && std::equal(l.begin(), l.end(), r.begin());
}

}  // namespace internal

}  // namespace base

template <typename T, size_t N, typename Ptr>
inline constexpr bool
    std::ranges::enable_borrowed_range<base::span<T, N, Ptr>> = true;

template <typename T, size_t N, typename Ptr>
inline constexpr bool std::ranges::enable_view<base::span<T, N, Ptr>> = true;

// EXTENT returns the size of any type that can be converted to a |base::span|
// with definite extent, i.e. everything that is a contiguous storage of some
// sort with static size. Specifically, this works for std::array in a constexpr
// context. Note:
//   * |std::size| should be preferred for plain arrays.
//   * In run-time contexts, functions such as |std::array::size| should be
//     preferred.
#define EXTENT(x)                                                          \
  ::base::internal::must_not_be_dynamic_extent<decltype(::base::make_span( \
      x))::extent>()

#endif  // MINI_CHROMIUM_BASE_CONTAINERS_SPAN_H_
