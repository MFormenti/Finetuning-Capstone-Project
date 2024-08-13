/* -*- c -*- */
/*
 * vim:syntax=c
 *
 * Low-level routines related to IEEE-754 format
 */
#include "numpy/utils.h"

#include "npy_math_common.h"
#include "npy_math_private.h"

/*
 The below code is provided for compilers which do not yet provide C11
 compatibility (gcc 4.5 and older)
 */
#ifndef LDBL_TRUE_MIN
#define LDBL_TRUE_MIN __LDBL_DENORM_MIN__
#endif


/*
 * FIXME: There is a lot of redundancy between _next* and npy_nextafter*.
 * refactor this at some point
 *
 * p >= 0, returnx x + nulp
 * p < 0, returnx x - nulp
 */
static double
_next(double x, int p)
{
    volatile double t;
    npy_int32 hx, hy, ix;
    npy_uint32 lx;

    EXTRACT_WORDS(hx, lx, x);
    ix = hx & 0x7fffffff; /* |x| */

    if (((ix >= 0x7ff00000) && ((ix - 0x7ff00000) | lx) != 0)) /* x is nan */
        return x;
    if ((ix | lx) == 0) { /* x == 0 */
        if (p >= 0) {
            INSERT_WORDS(x, 0x0, 1); /* return +minsubnormal */
        }
        else {
            INSERT_WORDS(x, 0x80000000, 1); /* return -minsubnormal */
        }
        t = x * x;
        if (t == x)
            return t;
        else
            return x; /* raise underflow flag */
    }
    if (p < 0) { /* x -= ulp */
        if (lx == 0)
            hx -= 1;
        lx -= 1;
    }
    else { /* x += ulp */
        lx += 1;
        if (lx == 0)
            hx += 1;
    }
    hy = hx & 0x7ff00000;
    if (hy >= 0x7ff00000)
        return x + x;      /* overflow  */
    if (hy < 0x00100000) { /* underflow */
        t = x * x;
        if (t != x) { /* raise underflow flag */
            INSERT_WORDS(x, hx, lx);
            return x;
        }
    }
    INSERT_WORDS(x, hx, lx);
    return x;
}

static float
_next(float x, int p)
{
    volatile float t;
    npy_int32 hx, hy, ix;

    GET_FLOAT_WORD(hx, x);
    ix = hx & 0x7fffffff; /* |x| */

    if ((ix > 0x7f800000)) /* x is nan */
        return x;
    if (ix == 0) { /* x == 0 */
        if (p >= 0) {
            SET_FLOAT_WORD(x, 0x0 | 1); /* return +minsubnormal */
        }
        else {
            SET_FLOAT_WORD(x, 0x80000000 | 1); /* return -minsubnormal */
        }
        t = x * x;
        if (t == x)
            return t;
        else
            return x; /* raise underflow flag */
    }
    if (p < 0) { /* x -= ulp */
        hx -= 1;
    }
    else { /* x += ulp */
        hx += 1;
    }
    hy = hx & 0x7f800000;
    if (hy >= 0x7f800000)
        return x + x;      /* overflow  */
    if (hy < 0x00800000) { /* underflow */
        t = x * x;
        if (t != x) { /* raise underflow flag */
            SET_FLOAT_WORD(x, hx);
            return x;
        }
    }
    SET_FLOAT_WORD(x, hx);
    return x;
}

#if defined(HAVE_LDOUBLE_DOUBLE_DOUBLE_BE) || \
        defined(HAVE_LDOUBLE_DOUBLE_DOUBLE_LE)

/*
 * FIXME: this is ugly and untested. The asm part only works with gcc, and we
 * should consolidate the GET_LDOUBLE* / SET_LDOUBLE macros
 */
#define math_opt_barrier(x)    \
    ({                         \
        __typeof(x) __x = x;   \
        __asm("" : "+m"(__x)); \
        __x;                   \
    })
#define math_force_eval(x) __asm __volatile("" : : "m"(x))

/* only works for big endian */
typedef union {
    npy_longdouble value;
    struct {
        npy_uint64 msw;
        npy_uint64 lsw;
    } parts64;
    struct {
        npy_uint32 w0, w1, w2, w3;
    } parts32;
} ieee854_long_double_shape_type;

/* Get two 64 bit ints from a long double.  */

#define GET_LDOUBLE_WORDS64(ix0, ix1, d)     \
    do {                                     \
        ieee854_long_double_shape_type qw_u; \
        qw_u.value = (d);                    \
        (ix0) = qw_u.parts64.msw;            \
        (ix1) = qw_u.parts64.lsw;            \
    } while (0)

/* Set a long double from two 64 bit ints.  */

#define SET_LDOUBLE_WORDS64(d, ix0, ix1)     \
    do {                                     \
        ieee854_long_double_shape_type qw_u; \
        qw_u.parts64.msw = (ix0);            \
        qw_u.parts64.lsw = (ix1);            \
        (d) = qw_u.value;                    \
    } while (0)

static long double
_next(long double x, int p)
{
    npy_int64 hx, ihx, ilx;
    npy_uint64 lx;
    npy_longdouble u;
    const npy_longdouble eps = exp2l(-105.); // 0x1.0000000000000p-105L

    GET_LDOUBLE_WORDS64(hx, lx, x);
    ihx = hx & 0x7fffffffffffffffLL; /* |hx| */
    ilx = lx & 0x7fffffffffffffffLL; /* |lx| */

    if (((ihx & 0x7ff0000000000000LL) == 0x7ff0000000000000LL) &&
        ((ihx & 0x000fffffffffffffLL) != 0)) {
        return x; /* signal the nan */
    }
    if (ihx == 0 && ilx == 0) {          /* x == 0 */
        SET_LDOUBLE_WORDS64(x, p, 0ULL); /* return +-minsubnormal */
        u = x * x;
        if (u == x) {
            return u;
        }
        else {
            return x; /* raise underflow flag */
        }
    }

    if (p < 0) { /* p < 0, x -= ulp */
        if ((hx == 0xffefffffffffffffLL) && (lx == 0xfc8ffffffffffffeLL))
            return x + x; /* overflow, return -inf */
        if (hx >= 0x7ff0000000000000LL) {
            SET_LDOUBLE_WORDS64(u, 0x7fefffffffffffffLL, 0x7c8ffffffffffffeLL);
            return u;
        }
        if (ihx <= 0x0360000000000000LL) { /* x <= LDBL_MIN */
            u = math_opt_barrier(x);
            x -= LDBL_TRUE_MIN;
            if (ihx < 0x0360000000000000LL || (hx > 0 && (npy_int64)lx <= 0) ||
                (hx < 0 && (npy_int64)lx > 1)) {
                u = u * u;
                math_force_eval(u); /* raise underflow flag */
            }
            return x;
        }
        if (ihx < 0x06a0000000000000LL) { /* ulp will denormal */
            SET_LDOUBLE_WORDS64(u, (hx & 0x7ff0000000000000LL), 0ULL);
            u *= eps;
        }
        else
            SET_LDOUBLE_WORDS64(
                    u, (hx & 0x7ff0000000000000LL) - 0x0690000000000000LL,
                    0ULL);
        return x - u;
    }
    else { /* p >= 0, x += ulp */
        if ((hx == 0x7fefffffffffffffLL) && (lx == 0x7c8ffffffffffffeLL))
            return x + x; /* overflow, return +inf */
        if ((npy_uint64)hx >= 0xfff0000000000000ULL) {
            SET_LDOUBLE_WORDS64(u, 0xffefffffffffffffLL, 0xfc8ffffffffffffeLL);
            return u;
        }
        if (ihx <= 0x0360000000000000LL) { /* x <= LDBL_MIN */
            u = math_opt_barrier(x);
            x += LDBL_TRUE_MIN;
            if (ihx < 0x0360000000000000LL ||
                (hx > 0 && (npy_int64)lx < 0 && lx != 0x8000000000000001LL) ||
                (hx < 0 && (npy_int64)lx >= 0)) {
                u = u * u;
                math_force_eval(u); /* raise underflow flag */
            }
            if (x == 0.0L) /* handle negative LDBL_TRUE_MIN case */
                x = -0.0L;
            return x;
        }
        if (ihx < 0x06a0000000000000LL) { /* ulp will denormal */
            SET_LDOUBLE_WORDS64(u, (hx & 0x7ff0000000000000LL), 0ULL);
            u *= eps;
        }
        else
            SET_LDOUBLE_WORDS64(
                    u, (hx & 0x7ff0000000000000LL) - 0x0690000000000000LL,
                    0ULL);
        return x + u;
    }
}
#else
static long double
_next(long double x, int p)
{
    volatile npy_longdouble t;
    union IEEEl2bitsrep ux;

    ux.e = x;

    if ((GET_LDOUBLE_EXP(ux) == 0x7fff &&
         ((GET_LDOUBLE_MANH(ux) & ~LDBL_NBIT) | GET_LDOUBLE_MANL(ux)) != 0)) {
        return ux.e; /* x is nan */
    }
    if (ux.e == 0.0) {
        SET_LDOUBLE_MANH(ux, 0); /* return +-minsubnormal */
        SET_LDOUBLE_MANL(ux, 1);
        if (p >= 0) {
            SET_LDOUBLE_SIGN(ux, 0);
        }
        else {
            SET_LDOUBLE_SIGN(ux, 1);
        }
        t = ux.e * ux.e;
        if (t == ux.e) {
            return t;
        }
        else {
            return ux.e; /* raise underflow flag */
        }
    }
    if (p < 0) { /* x -= ulp */
        if (GET_LDOUBLE_MANL(ux) == 0) {
            if ((GET_LDOUBLE_MANH(ux) & ~LDBL_NBIT) == 0) {
                SET_LDOUBLE_EXP(ux, GET_LDOUBLE_EXP(ux) - 1);
            }
            SET_LDOUBLE_MANH(ux, (GET_LDOUBLE_MANH(ux) - 1) |
                                         (GET_LDOUBLE_MANH(ux) & LDBL_NBIT));
        }
        SET_LDOUBLE_MANL(ux, GET_LDOUBLE_MANL(ux) - 1);
    }
    else { /* x += ulp */
        SET_LDOUBLE_MANL(ux, GET_LDOUBLE_MANL(ux) + 1);
        if (GET_LDOUBLE_MANL(ux) == 0) {
            SET_LDOUBLE_MANH(ux, (GET_LDOUBLE_MANH(ux) + 1) |
                                         (GET_LDOUBLE_MANH(ux) & LDBL_NBIT));
            if ((GET_LDOUBLE_MANH(ux) & ~LDBL_NBIT) == 0) {
                SET_LDOUBLE_EXP(ux, GET_LDOUBLE_EXP(ux) + 1);
            }
        }
    }
    if (GET_LDOUBLE_EXP(ux) == 0x7fff) {
        return ux.e + ux.e; /* overflow  */
    }
    if (GET_LDOUBLE_EXP(ux) == 0) { /* underflow */
        if (LDBL_NBIT) {
            SET_LDOUBLE_MANH(ux, GET_LDOUBLE_MANH(ux) & ~LDBL_NBIT);
        }
        t = ux.e * ux.e;
        if (t != ux.e) { /* raise underflow flag */
            return ux.e;
        }
    }

    return ux.e;
}
#endif

namespace {
template <typename T>
struct numeric_limits;

template <>
struct numeric_limits<float> {
    static const npy_float nan;
};
const npy_float numeric_limits<float>::nan = NPY_NANF;

template <>
struct numeric_limits<double> {
    static const npy_double nan;
};
const npy_double numeric_limits<double>::nan = NPY_NAN;

template <>
struct numeric_limits<long double> {
    static const npy_longdouble nan;
};
const npy_longdouble numeric_limits<long double>::nan = NPY_NANL;
}  // namespace

template <typename type>
static type
_npy_spacing(type x)
{
    /* XXX: npy isnan/isinf may be optimized by bit twiddling */
    if (npy_isinf(x)) {
        return numeric_limits<type>::nan;
    }

    return _next(x, 1) - x;
}

/*
 * Instantiation of C interface
 */
extern "C" {
npy_float
npy_spacingf(npy_float x)
{
    return _npy_spacing(x);
}
npy_double
npy_spacing(npy_double x)
{
    return _npy_spacing(x);
}
npy_longdouble
npy_spacingl(npy_longdouble x)
{
    return _npy_spacing(x);
}
}

extern "C" int
npy_clear_floatstatus()
{
    char x = 0;
    return npy_clear_floatstatus_barrier(&x);
}
extern "C" int
npy_get_floatstatus()
{
    char x = 0;
    return npy_get_floatstatus_barrier(&x);
}


/* 
 * General C99 code for floating point error handling.  These functions mainly
 * exists, because `fenv.h` was not standardized in C89 so they gave better
 * portability.  This should be unnecessary with C99/C++11 and further
 * functionality can be used from `fenv.h` directly. 
 */
#include <fenv.h>

/*
 * According to the C99 standard FE_DIVBYZERO, etc. may not be provided when
 * unsupported.  In such cases NumPy will not report these correctly, but we
 * should still allow compiling (whether tests pass or not).
 * By defining them as 0 locally, we make them no-ops.  Unlike these defines,
 * for example `musl` still defines all of the functions (as no-ops):
 *     https://git.musl-libc.org/cgit/musl/tree/src/fenv/fenv.c
 * and does similar replacement in its tests:
 * http://nsz.repo.hu/git/?p=libc-test;a=blob;f=src/common/mtest.h;h=706c1ba23ea8989b17a2f72ed1a919e187c06b6a;hb=HEAD#l30
 */
#ifndef FE_DIVBYZERO
    #define FE_DIVBYZERO 0
#endif
#ifndef FE_OVERFLOW
    #define FE_OVERFLOW 0
#endif
#ifndef FE_UNDERFLOW
    #define FE_UNDERFLOW 0
#endif
#ifndef FE_INVALID
    #define FE_INVALID 0
#endif


extern "C" int
npy_get_floatstatus_barrier(char *param)
{
    int fpstatus = fetestexcept(FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW |
                                FE_INVALID);
    /*
     * By using a volatile, the compiler cannot reorder this call
     */
    if (param != NULL) {
        volatile char NPY_UNUSED(c) = *(char *)param;
    }

    return ((FE_DIVBYZERO & fpstatus) ? NPY_FPE_DIVIDEBYZERO : 0) |
           ((FE_OVERFLOW & fpstatus) ? NPY_FPE_OVERFLOW : 0) |
           ((FE_UNDERFLOW & fpstatus) ? NPY_FPE_UNDERFLOW : 0) |
           ((FE_INVALID & fpstatus) ? NPY_FPE_INVALID : 0);
}

extern "C" int
npy_clear_floatstatus_barrier(char *param)
{
    /* testing float status is 50-100 times faster than clearing on x86 */
    int fpstatus = npy_get_floatstatus_barrier(param);
    if (fpstatus != 0) {
        feclearexcept(FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INVALID);
    }

    return fpstatus;
}

extern "C" void
npy_set_floatstatus_divbyzero(void)
{
    feraiseexcept(FE_DIVBYZERO);
}

extern "C" void
npy_set_floatstatus_overflow(void)
{
    feraiseexcept(FE_OVERFLOW);
}

extern "C" void
npy_set_floatstatus_underflow(void)
{
    feraiseexcept(FE_UNDERFLOW);
}

extern "C" void
npy_set_floatstatus_invalid(void)
{
    feraiseexcept(FE_INVALID);
}
