/**
 * @file gmath.h
 */

#include <math.h>

typedef float f32;
typedef double f64;

// ================================================================================================================================

[[maybe_unused]] static inline f32 f32_digits(const f32 x) { return floorf(log10f(fmaxf(fabsf(x), 1.0f))) + 1.0f; }
[[maybe_unused]] static inline f64 f64_digits(const f64 x) { return floor (log10 (fmax (fabs (x), 1.0 ))) + 1.0 ; }

[[maybe_unused]] static inline f32 f32_fract(const f32 x) { return x - floorf(x); }
[[maybe_unused]] static inline f64 f64_fract(const f64 x) { return x - floor (x); }

[[maybe_unused]] static inline f32 f32_round(const f32 x) { return f32_fract(x) < 0.5f ? floorf(x) : ceilf(x); }
[[maybe_unused]] static inline f64 f64_round(const f64 x) { return f64_fract(x) < 0.5  ? floor (x) : ceil (x); }

[[maybe_unused]] static inline f32 f32_modf(const f32 a, const f32 b) { return a - b * floorf(a / b); }
[[maybe_unused]] static inline f64 f64_modf(const f64 a, const f64 b) { return a - b * floor (a / b); }

[[maybe_unused]] static inline f32 f32_modr(const f32 a, const f32 b) { return a - b * f32_round(a / b); }
[[maybe_unused]] static inline f64 f64_modr(const f64 a, const f64 b) { return a - b * f64_round(a / b); }

[[maybe_unused]] static inline f32 f32_triwave(const f32 x) { return fabsf(f32_modf(2.0f * x, 2.0f) - 1.0f); }
[[maybe_unused]] static inline f64 f64_triwave(const f64 x) { return fabs (f64_modf(2.0  * x, 2.0 ) - 1.0 ); }

// ================================================================================================================================
