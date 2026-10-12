#ifndef KEEB_SIMD_H
#define KEEB_SIMD_H

#if defined(KEEB_ENABLE_NEON) && (KEEB_ENABLE_NEON == 0)
#define KEEB_NEON 0
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define KEEB_NEON 1
#include <arm_neon.h>
#else
#define KEEB_NEON 0
#endif

#endif
