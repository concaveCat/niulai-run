/* Minimal dependency-free CHECK macros for the Niulai host tests.
 * No Unity/CMake/external libs: plain gcc -std=c11 -Wall -Wextra -Werror. */
#ifndef QW_CHECK_H
#define QW_CHECK_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int qw_checks_run = 0;

#define QW_FAIL_MSG(fmt, ...)                                                 \
    do {                                                                      \
        fprintf(stderr, "FAIL %s:%d: " fmt "\n", __FILE__, __LINE__,          \
                __VA_ARGS__);                                                 \
        exit(1);                                                              \
    } while (0)

#define CHECK(cond)                                                           \
    do {                                                                      \
        qw_checks_run++;                                                      \
        if (!(cond)) {                                                        \
            QW_FAIL_MSG("%s", #cond);                                         \
        }                                                                     \
    } while (0)

#define CHECK_INT_EQ(actual, expected)                                        \
    do {                                                                      \
        qw_checks_run++;                                                      \
        long long a_ = (long long)(actual);                                   \
        long long e_ = (long long)(expected);                                 \
        if (a_ != e_) {                                                       \
            QW_FAIL_MSG("%s == %s (%lld != %lld)", #actual, #expected, a_,   \
                        e_);                                                  \
        }                                                                     \
    } while (0)

#define CHECK_FLOAT_NEAR(actual, expected, eps)                               \
    do {                                                                      \
        qw_checks_run++;                                                      \
        double a_ = (double)(actual);                                         \
        double e_ = (double)(expected);                                       \
        double p_ = (double)(eps);                                            \
        if (!(fabs(a_ - e_) <= p_)) {                                         \
            QW_FAIL_MSG("%s ~= %s (%g !~= %g)", #actual, #expected, a_, e_); \
        }                                                                     \
    } while (0)

#define CHECK_STR_EQ(actual, expected)                                        \
    do {                                                                      \
        qw_checks_run++;                                                      \
        const char *a_ = (actual);                                            \
        const char *e_ = (expected);                                          \
        if (a_ == NULL || strcmp(a_, e_) != 0) {                              \
            QW_FAIL_MSG("%s == %s (\"%s\" != \"%s\")", #actual, #expected,    \
                        a_ != NULL ? a_ : "(null)", e_);                      \
        }                                                                     \
    } while (0)

#define RUN_TEST(fn)                                                          \
    do {                                                                      \
        printf("[ RUN  ] %s\n", #fn);                                         \
        int before_ = qw_checks_run;                                          \
        (fn)();                                                               \
        printf("[ PASS ] %s (%d checks)\n", #fn, qw_checks_run - before_);    \
    } while (0)

#endif /* QW_CHECK_H */
