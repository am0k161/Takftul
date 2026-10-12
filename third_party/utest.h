/*
   The latest version of this library is available on GitHub;
   https://github.com/sheredom/utest.h
*/

/*
   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any
   means.

   In jurisdictions that recognize copyright laws, the author or authors
   of this software dedicate any and all copyright interest in the
   software to the public domain. We make this dedication for the benefit
   of the public at large and to the detriment of our heirs and
   successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   For more information, please refer to <http://unlicense.org/>
*/

#ifndef SHEREDOM_UTEST_H_INCLUDED
#define SHEREDOM_UTEST_H_INCLUDED

#ifdef _MSC_VER
#pragma warning(push, 1)
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(__cplusplus)
#define UTEST_CAST(type, x) static_cast<type>(x)
#define UTEST_PTR_CAST(type, x) reinterpret_cast<type>(x)
#define UTEST_EXTERN extern "C"
#define UTEST_NULL NULL
#else
#define UTEST_CAST(type, x) ((type)(x))
#define UTEST_PTR_CAST(type, x) ((type)(x))
#define UTEST_EXTERN extern
#define UTEST_NULL 0
#endif

#if defined(_MSC_VER)
typedef __int64 utest_int64_t;
typedef unsigned __int64 utest_uint64_t;
typedef unsigned __int32 utest_uint32_t;
#else
#include <stdint.h>
typedef int64_t utest_int64_t;
typedef uint64_t utest_uint64_t;
typedef uint32_t utest_uint32_t;
#endif

struct utest_test_state_s {
  void (*func)(int *, size_t);
  size_t index;
  char *name;
};

struct utest_state_s {
  struct utest_test_state_s *tests;
  size_t tests_length;
  FILE *output;
};

#if defined(_MSC_VER)
#define UTEST_WEAK __forceinline
#elif defined(__clang__)
#define UTEST_WEAK __attribute__((weak))
#elif defined(__GNUC__)
#define UTEST_WEAK __attribute__((weak))
#else
#define UTEST_WEAK
#endif

#if defined(_MSC_VER)
#define UTEST_UNUSED
#else
#define UTEST_UNUSED __attribute__((unused))
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wvariadic-macros"
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif

UTEST_WEAK UTEST_UNUSED struct utest_state_s utest_state;

#define UTEST_PRINTF(...)                                                      \
  if (utest_state.output) {                                                    \
    fprintf(utest_state.output, __VA_ARGS__);                                  \
  }                                                                            \
  printf(__VA_ARGS__)

#if defined(__cplusplus)
#define UTEST_AUTO(x) auto
#else
#define UTEST_AUTO(x) __typeof__(x)
#endif

#define UTEST(SET, NAME)                                                       \
  static void utest_run_##SET##_##NAME(int *utest_result, size_t);             \
  static void utest_##SET##_##NAME(int *utest_result, size_t utest_index) {    \
    (void)utest_index;                                                         \
    utest_run_##SET##_##NAME(utest_result, utest_index);                       \
  }                                                                            \
  struct utest_register_##SET##_##NAME {                                       \
    utest_register_##SET##_##NAME() {                                          \
      const size_t index = utest_state.tests_length++;                         \
      const char *name_part = #SET "." #NAME;                                  \
      const size_t name_size = strlen(name_part) + 1;                          \
      char *name = UTEST_PTR_CAST(char *, malloc(name_size));                  \
      utest_state.tests = UTEST_PTR_CAST(                                      \
          struct utest_test_state_s *,                                         \
          realloc(UTEST_PTR_CAST(void *, utest_state.tests),                   \
                  sizeof(struct utest_test_state_s) *                          \
                      utest_state.tests_length));                              \
      utest_state.tests[index].func = &utest_##SET##_##NAME;                   \
      utest_state.tests[index].name = name;                                    \
      utest_state.tests[index].index = 0;                                      \
      memcpy(name, name_part, name_size);                                      \
    }                                                                          \
  };                                                                           \
  static utest_register_##SET##_##NAME utest_register_##SET##_##NAME##_instance; \
  void utest_run_##SET##_##NAME(int *utest_result, size_t utest_index)

#define ASSERT_EQ(x, y)                                                        \
  do {                                                                         \
    UTEST_AUTO(x) xEval = (x);                                                 \
    UTEST_AUTO(y) yEval = (y);                                                 \
    if (xEval != yEval) {                                                      \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %s == %s\n", #x, #y);                         \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_NE(x, y)                                                        \
  do {                                                                         \
    UTEST_AUTO(x) xEval = (x);                                                 \
    UTEST_AUTO(y) yEval = (y);                                                 \
    if (xEval == yEval) {                                                      \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %s != %s\n", #x, #y);                         \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(x)                                                         \
  do {                                                                         \
    if (!(x)) {                                                                \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : true\n");                                     \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_FALSE(x)                                                        \
  do {                                                                         \
    if (x) {                                                                   \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : false\n");                                    \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_LT(x, y)                                                        \
  do {                                                                         \
    UTEST_AUTO(x) xEval = (x);                                                 \
    UTEST_AUTO(y) yEval = (y);                                                 \
    if (!(xEval < yEval)) {                                                    \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %s < %s\n", #x, #y);                          \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_LE(x, y)                                                        \
  do {                                                                         \
    UTEST_AUTO(x) xEval = (x);                                                 \
    UTEST_AUTO(y) yEval = (y);                                                 \
    if (!(xEval <= yEval)) {                                                   \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %s <= %s\n", #x, #y);                         \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_GT(x, y)                                                        \
  do {                                                                         \
    UTEST_AUTO(x) xEval = (x);                                                 \
    UTEST_AUTO(y) yEval = (y);                                                 \
    if (!(xEval > yEval)) {                                                    \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %s > %s\n", #x, #y);                          \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_GE(x, y)                                                        \
  do {                                                                         \
    UTEST_AUTO(x) xEval = (x);                                                 \
    UTEST_AUTO(y) yEval = (y);                                                 \
    if (!(xEval >= yEval)) {                                                   \
      UTEST_PRINTF("%s:%i: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %s >= %s\n", #x, #y);                         \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define EXPECT_EQ(x, y) ASSERT_EQ(x, y)
#define EXPECT_NE(x, y) ASSERT_NE(x, y)
#define EXPECT_TRUE(x) ASSERT_TRUE(x)
#define EXPECT_FALSE(x) ASSERT_FALSE(x)
#define EXPECT_LT(x, y) ASSERT_LT(x, y)
#define EXPECT_LE(x, y) ASSERT_LE(x, y)
#define EXPECT_GT(x, y) ASSERT_GT(x, y)
#define EXPECT_GE(x, y) ASSERT_GE(x, y)

#define UTEST_MAIN()                                                           \
  int main(int argc, const char *const argv[]) {                               \
    size_t failed = 0;                                                         \
    size_t index = 0;                                                          \
    size_t *failed_testcases = 0;                                              \
    size_t failed_testcases_length = 0;                                        \
    (void)argc;                                                                \
    (void)argv;                                                                \
    printf("taktful tests\n");                                                    \
    for (index = 0; index < utest_state.tests_length; ++index) {               \
      int result = 0;                                                          \
      printf("[ RUN      ] %s\n", utest_state.tests[index].name);              \
      utest_state.tests[index].func(&result,                                   \
                                    utest_state.tests[index].index);           \
      if (0 != result) {                                                       \
        const size_t failed_testcase_index = failed_testcases_length++;        \
        failed_testcases = UTEST_PTR_CAST(                                     \
            size_t *, realloc(UTEST_PTR_CAST(void *, failed_testcases),        \
                              sizeof(size_t) * failed_testcases_length));      \
        failed_testcases[failed_testcase_index] = index;                       \
        failed++;                                                              \
        printf("[  FAILED  ] %s\n", utest_state.tests[index].name);            \
      } else {                                                                 \
        printf("[       OK ] %s\n", utest_state.tests[index].name);            \
      }                                                                        \
    }                                                                          \
    printf("\n%zu tests, %zu failed\n", utest_state.tests_length, failed);     \
    for (index = 0; index < failed_testcases_length; ++index) {                \
      printf("  %s\n", utest_state.tests[failed_testcases[index]].name);       \
    }                                                                          \
    for (index = 0; index < utest_state.tests_length; ++index) {               \
      free(UTEST_PTR_CAST(void *, utest_state.tests[index].name));             \
    }                                                                          \
    free(UTEST_PTR_CAST(void *, failed_testcases));                            \
    free(UTEST_PTR_CAST(void *, utest_state.tests));                           \
    return (failed == 0) ? 0 : 1;                                              \
  }

#endif
