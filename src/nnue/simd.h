#ifndef SIMD_H
#define SIMD_H

#include <cstdint>
#include <immintrin.h>

namespace NNUE {

#ifdef __AVX2__

// Wrapper for AVX2 256-bit integer vector
class Integer256 {
public:
  __m256i val;

  // Constructors
  Integer256() : val(_mm256_setzero_si256()) {}
  Integer256(__m256i v) : val(v) {}
  Integer256(const int16_t *ptr)
      : val(_mm256_load_si256((const __m256i *)ptr)) {}

  // Load/Store
  static Integer256 load(const int16_t *ptr) {
    return Integer256(_mm256_load_si256((const __m256i *)ptr));
  }

  static Integer256 load_unaligned(const int16_t *ptr) {
    return Integer256(_mm256_loadu_si256((const __m256i *)ptr));
  }

  void store(int16_t *ptr) const { _mm256_store_si256((__m256i *)ptr, val); }

  // Arithmetic
  // Add 16-bit integers
  Integer256 operator+(const Integer256 &other) const {
    return Integer256(_mm256_add_epi16(val, other.val));
  }

  Integer256 &operator+=(const Integer256 &other) {
    val = _mm256_add_epi16(val, other.val);
    return *this;
  }

  // Subtract 16-bit integers
  Integer256 operator-(const Integer256 &other) const {
    return Integer256(_mm256_sub_epi16(val, other.val));
  }

  Integer256 &operator-=(const Integer256 &other) {
    val = _mm256_sub_epi16(val, other.val);
    return *this;
  }

  // Max (for clipped ReLU / Clamp)
  static Integer256 max(const Integer256 &a, const Integer256 &b) {
    return Integer256(_mm256_max_epi16(a.val, b.val));
  }

  // Min
  static Integer256 min(const Integer256 &a, const Integer256 &b) {
    return Integer256(_mm256_min_epi16(a.val, b.val));
  }

  // Zero
  static Integer256 zero() { return Integer256(_mm256_setzero_si256()); }
};

#else
// Fallback for non-AVX2 (scalar) - Minimal implementation for compilation
// In production, we should probably enforcing AVX2 or provide full scalar
// fallback.
struct Integer256 {
  // Placeholder for compilation if AVX2 is missing
  // Real logic would be needed here for non-AVX builds.
};
#endif

} // namespace NNUE

#endif // SIMD_H
