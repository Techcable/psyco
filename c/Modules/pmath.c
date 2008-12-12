/* 
 * Psyco version of python math module, mathmodule.c 
 *
 * TODO: 
 * - implement:
 *   - frexp()
 *   - ldexp()
 *   - log()
 *   - log10()
 *   - modf()
 *   - degrees()
 *   - radians()
 * ... etc. See the method table of Python 2.6's mathmodule.c
 */

#include "../initialize.h"
#include "../Python/pyver.h"
#if HAVE_FP_FN_CALLS    /* disable float optimizations if functions with
                           float/double arguments are not implemented
                           in the back-end */

#ifndef _MSC_VER
#ifndef __STDC__
extern double fmod (double, double);
extern double frexp (double, int *);
extern double ldexp (double, int);
extern double modf (double, double *);
#endif /* __STDC__ */
#endif /* _MSC_VER */


#include <math.h>

/* taken from Python 2.6's mathmodule.c */

/* Call is_error when errno != 0, and where x is the result libm
 * returned.  is_error will usually set up an exception and return
 * true (1), but may return false (0) without setting up an exception.
 */
static int
is_error(double x)
{
	int result = 1;	/* presumption of guilt */
	assert(errno);	/* non-zero errno is a precondition for calling */
	if (errno == EDOM)
		PyErr_SetString(PyExc_ValueError, "math domain error");

	else if (errno == ERANGE) {
		/* ANSI C generally requires libm functions to set ERANGE
		 * on overflow, but also generally *allows* them to set
		 * ERANGE on underflow too.  There's no consistency about
		 * the latter across platforms.
		 * Alas, C99 never requires that errno be set.
		 * Here we suppress the underflow errors (libm functions
		 * should return a zero on underflow, and +- HUGE_VAL on
		 * overflow, so testing the result for zero suffices to
		 * distinguish the cases).
		 *
		 * On some platforms (Ubuntu/ia64) it seems that errno can be
		 * set to ERANGE for subnormal results that do *not* underflow
		 * to zero.  So to be safe, we'll ignore ERANGE whenever the
		 * function result is less than one in absolute value.
		 */
		if (fabs(x) < 1.0)
			result = 0;
		else
			PyErr_SetString(PyExc_OverflowError,
					"math range error");
	}
	else
                /* Unexpected math error */
		PyErr_SetFromErrno(PyExc_ValueError);
	return result;
}

#ifdef HAVE_COPYSIGN
/* only python >= 2.6 has the extra defs */

/*
   wrapper for atan2 that deals directly with special cases before
   delegating to the platform libm for the remaining cases.  This
   is necessary to get consistent behaviour across platforms.
   Windows, FreeBSD and alpha Tru64 are amongst platforms that don't
   always follow C99.
*/

static double
m_atan2(double y, double x)
{
	if (Py_IS_NAN(x) || Py_IS_NAN(y))
		return Py_NAN;
	if (Py_IS_INFINITY(y)) {
		if (Py_IS_INFINITY(x)) {
			if (copysign(1., x) == 1.)
				/* atan2(+-inf, +inf) == +-pi/4 */
				return copysign(0.25*Py_MATH_PI, y);
			else
				/* atan2(+-inf, -inf) == +-pi*3/4 */
				return copysign(0.75*Py_MATH_PI, y);
		}
		/* atan2(+-inf, x) == +-pi/2 for finite x */
		return copysign(0.5*Py_MATH_PI, y);
	}
	if (Py_IS_INFINITY(x) || y == 0.) {
		if (copysign(1., x) == 1.)
			/* atan2(+-y, +inf) = atan2(+-0, +x) = +-0. */
			return copysign(0., y);
		else
			/* atan2(+-y, -inf) = atan2(+-0., -x) = +-pi. */
			return copysign(Py_MATH_PI, y);
	}
	return atan2(y, x);
}

#else
#define m_atan2 atan2
#endif

#define CIMPL_MATH_FUNC1(funcname, func, libfunc, can_overflow) \
    static int cimpl_math_##func(double a, double* result) {	\
        errno = 0;						\
        PyFPE_START_PROTECT(funcname, return -1)		\
        *result = libfunc(a);					\
        PyFPE_END_PROTECT(*result);				\
	if (Py_IS_NAN(*result)) {				\
		if (!Py_IS_NAN(a))				\
			errno = EDOM;				\
		else						\
			errno = 0;				\
	}							\
	else if (Py_IS_INFINITY(*result)) {			\
		if (Py_IS_FINITE(a))				\
			errno = can_overflow ? ERANGE : EDOM;	\
		else						\
			errno = 0;				\
	}							\
	if (errno && is_error(*result))				\
		return -1;					\
        return 0;						\
    }

#define CIMPL_MATH_FUNC2(funcname, func, libfunc) \
    static int cimpl_math_##func(double a, double b, double* result) { \
        errno = 0;						\
        PyFPE_START_PROTECT(funcname, return -1)		\
        *result = libfunc(a,b);					\
        PyFPE_END_PROTECT(*result);				\
	if (Py_IS_NAN(*result)) {				\
		if (!Py_IS_NAN(a) && !Py_IS_NAN(b))		\
			errno = EDOM;				\
		else						\
			errno = 0;				\
	}							\
	else if (Py_IS_INFINITY(*result)) {			\
		if (Py_IS_FINITE(a) && Py_IS_FINITE(b))		\
			errno = ERANGE;				\
		else						\
			errno = 0;				\
	}							\
	if (errno && is_error(*result))				\
		return -1;					\
        return 0;						\
    }

/* 
 * This is almost but not quite the same as the 
 * version in pfloatobject.c. The error handling 
 * on invalid args is different
 */
#define PMATH_CONVERT_TO_DOUBLE(vobj, v1, v2)			\
    switch (psyco_convert_to_double(po, vobj, &v1, &v2)) {	\
    case true:							\
        break;   /* fine */					\
    case false:							\
        goto error;  /* error or promotion */			\
    default:							\
        goto fallback;  /* e.g. not a float object */		\
    }

#define PMATH_RELEASE_DOUBLE(v1, v2) \
    vinfo_decref(v2, po); \
    vinfo_decref(v1, po);

#if MATHMODULE_USES_METH_O    /* Python >= 2.6 */
# define PMATH_FUNC1(funcname, func ) \
    static PyCFunction fallback_##func; \
    static vinfo_t* pmath_##func(PsycoObject* po, vinfo_t* vself, vinfo_t* v) { \
        vinfo_t *a1, *a2, *x; \
        vinfo_array_t* result; \
        PMATH_CONVERT_TO_DOUBLE(v,a1,a2); \
        result = array_new(2); \
        x = psyco_generic_call(po, cimpl_math_##func, CfPure|CfNoReturnValue|CfPyErrIfNonNull, \
                                  "vva",a1,a2,result); \
        PMATH_RELEASE_DOUBLE(a1,a2); \
        if (x != NULL) \
            x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]); \
        array_release(result); \
        return x; \
    fallback:                                                   \
        return psyco_generic_call(po, fallback_##func,          \
                                  CfReturnRef|CfPyErrIfNull,    \
                                  "lv", NULL, v);               \
    error:                                                      \
        return NULL;                                            \
    }
#else
# define PMATH_FUNC1(funcname, func ) \
    static PyCFunction fallback_##func; \
    static vinfo_t* pmath_##func(PsycoObject* po, vinfo_t* vself, vinfo_t* varg) { \
        vinfo_t *a1, *a2, *x; \
        vinfo_array_t* result; \
        vinfo_t *v; \
        int tuplesize = PsycoTuple_Load(varg); \
        if (tuplesize != 1) /* wrong or unknown number of arguments */ \
          goto fallback; \
        v = PsycoTuple_GET_ITEM(varg, 0); \
        PMATH_CONVERT_TO_DOUBLE(v,a1,a2); \
        result = array_new(2); \
        x = psyco_generic_call(po, cimpl_math_##func, CfPure|CfNoReturnValue|CfPyErrIfNonNull, \
                                  "vva",a1,a2,result); \
        PMATH_RELEASE_DOUBLE(a1,a2); \
        if (x != NULL) \
            x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]); \
        array_release(result); \
        return x; \
    fallback:                                                   \
        return psyco_generic_call(po, fallback_##func,          \
                                  CfReturnRef|CfPyErrIfNull,    \
                                  "lv", NULL, varg);            \
    error:                                                      \
        return NULL;                                            \
    }
#endif

#define PMATH_FUNC2(funcname, func ) \
    static PyCFunction fallback_##func; \
    static vinfo_t* pmath_##func(PsycoObject* po, vinfo_t* vself, vinfo_t* varg) { \
        vinfo_t *a1 = NULL, *a2 = NULL, *b1, *b2, *x; \
        vinfo_array_t* result; \
        vinfo_t *v1, *v2; \
        int tuplesize = PsycoTuple_Load(varg); \
        if (tuplesize != 2) /* wrong or unknown number of arguments */ \
          goto fallback; \
        v1 = PsycoTuple_GET_ITEM(varg, 0); \
        v2 = PsycoTuple_GET_ITEM(varg, 1); \
        PMATH_CONVERT_TO_DOUBLE(v1,a1,a2); \
        PMATH_CONVERT_TO_DOUBLE(v2,b1,b2); \
        result = array_new(2); \
        x = psyco_generic_call(po, cimpl_math_##func, CfPure|CfNoReturnValue|CfPyErrIfNonNull, \
                               "vvvva",a1,a2,b1,b2,result); \
        PMATH_RELEASE_DOUBLE(a1,a2); \
        PMATH_RELEASE_DOUBLE(b1,b2); \
        if (x != NULL) \
            x = PsycoFloat_FROM_DOUBLE(result->items[0], result->items[1]); \
        array_release(result); \
        return x; \
    fallback:                                                   \
        return psyco_generic_call(po, fallback_##func,          \
                                  CfReturnRef|CfPyErrIfNull,    \
                                  "lv", NULL, varg);            \
    error:                                                      \
        if (a2 != NULL) {                                       \
          PMATH_RELEASE_DOUBLE(a1,a2);                          \
        }                                                       \
        return NULL;                                            \
    }

/* the functions cimpl_math_sin() etc */
CIMPL_MATH_FUNC1("acos", acos, acos, 0)
CIMPL_MATH_FUNC1("asin", asin, asin, 0)
CIMPL_MATH_FUNC1("atan", atan, atan, 0)
CIMPL_MATH_FUNC2("atan2", atan2, m_atan2)
CIMPL_MATH_FUNC1("ceil", ceil, ceil, 0)
CIMPL_MATH_FUNC1("cos", cos, cos, 0)
CIMPL_MATH_FUNC1("cosh", cosh, cosh, 1)
CIMPL_MATH_FUNC1("exp", exp, exp, 1)
CIMPL_MATH_FUNC1("fabs", fabs, fabs, 0)
CIMPL_MATH_FUNC1("floor", floor, floor, 0)
CIMPL_MATH_FUNC1("sin", sin, sin, 0)
CIMPL_MATH_FUNC1("sinh", sinh, sinh, 1)
CIMPL_MATH_FUNC1("sqrt", sqrt, sqrt, 0)
CIMPL_MATH_FUNC1("tan", tan, tan, 0)
CIMPL_MATH_FUNC1("tanh", tanh, tanh, 0)

static int
cimpl_math_fmod(double x, double y, double* result)
{
	/* fmod(x, +/-Inf) returns x for finite x. */
	if (Py_IS_INFINITY(y) && Py_IS_FINITE(x)) {
		*result = x;
		return 0;
	}
	errno = 0;
	PyFPE_START_PROTECT("in math_fmod", return -1);
	*result = fmod(x, y);
	PyFPE_END_PROTECT(*result);
	if (Py_IS_NAN(*result)) {
		if (!Py_IS_NAN(x) && !Py_IS_NAN(y))
			errno = EDOM;
		else
			errno = 0;
	}
	if (errno && is_error(*result))
		return -1;
	return 0;
}

static int
cimpl_math_hypot(double x, double y, double* result)
{
	/* hypot(x, +/-Inf) returns Inf, even if x is a NaN. */
	if (Py_IS_INFINITY(x)) {
		*result = fabs(x);
		return 0;
	}
	if (Py_IS_INFINITY(y)) {
		*result = fabs(y);
		return 0;
	}
	errno = 0;
	PyFPE_START_PROTECT("in math_hypot", return -1);
	*result = hypot(x, y);
	PyFPE_END_PROTECT(*result);
	if (Py_IS_NAN(*result)) {
		if (!Py_IS_NAN(x) && !Py_IS_NAN(y))
			errno = EDOM;
		else
			errno = 0;
	}
	else if (Py_IS_INFINITY(*result)) {
		if (Py_IS_FINITE(x) && Py_IS_FINITE(y))
			errno = ERANGE;
		else
			errno = 0;
	}
	if (errno && is_error(*result))
		return -1;
	return 0;
}

#ifdef HAVE_COPYSIGN

/* pow can't use math_2, but needs its own wrapper: the problem is
   that an infinite result can arise either as a result of overflow
   (in which case OverflowError should be raised) or as a result of
   e.g. 0.**-5. (for which ValueError needs to be raised.)
*/

static int
cimpl_math_pow(double x, double y, double* result)
{
	int odd_y;

	/* deal directly with IEEE specials, to cope with problems on various
	   platforms whose semantics don't exactly match C99 */
	*result = 0.; /* silence compiler warning */
	if (!Py_IS_FINITE(x) || !Py_IS_FINITE(y)) {
		errno = 0;
		if (Py_IS_NAN(x))
			*result = y == 0. ? 1. : x; /* NaN**0 = 1 */
		else if (Py_IS_NAN(y))
			*result = x == 1. ? 1. : y; /* 1**NaN = 1 */
		else if (Py_IS_INFINITY(x)) {
			odd_y = Py_IS_FINITE(y) && fmod(fabs(y), 2.0) == 1.0;
			if (y > 0.)
				*result = odd_y ? x : fabs(x);
			else if (y == 0.)
				*result = 1.;
			else /* y < 0. */
				*result = odd_y ? copysign(0., x) : 0.;
		}
		else if (Py_IS_INFINITY(y)) {
			if (fabs(x) == 1.0)
				*result = 1.;
			else if (y > 0. && fabs(x) > 1.0)
				*result = y;
			else if (y < 0. && fabs(x) < 1.0) {
				*result = -y; /* result is +inf */
				if (x == 0.) /* 0**-inf: divide-by-zero */
					errno = EDOM;
			}
			else
				*result = 0.;
		}
	}
	else {
		/* let libm handle finite**finite */
		errno = 0;
		PyFPE_START_PROTECT("in math_pow", return -1);
		*result = pow(x, y);
		PyFPE_END_PROTECT(*result);
		/* a NaN result should arise only from (-ve)**(finite
		   non-integer); in this case we want to raise ValueError. */
		if (!Py_IS_FINITE(*result)) {
			if (Py_IS_NAN(*result)) {
				errno = EDOM;
			}
			/* 
			   an infinite result here arises either from:
			   (A) (+/-0.)**negative (-> divide-by-zero)
			   (B) overflow of x**y with x and y finite
			*/
			else if (Py_IS_INFINITY(*result)) {
				if (x == 0.)
					errno = EDOM;
				else
					errno = ERANGE;
			}
		}
	}

	if (errno && is_error(*result))
		return -1;
	return 0;
}

#else
CIMPL_MATH_FUNC2("pow", pow, pow);
#endif

/* the functions pmath_sin() etc */
PMATH_FUNC1("acos", acos)
PMATH_FUNC1("asin", asin)
PMATH_FUNC1("atan", atan)
PMATH_FUNC2("atan2", atan2)
PMATH_FUNC1("ceil", ceil)
PMATH_FUNC1("cos", cos)
PMATH_FUNC1("cosh", cosh)
PMATH_FUNC1("exp", exp)
PMATH_FUNC1("fabs", fabs)
PMATH_FUNC1("floor", floor)
PMATH_FUNC2("fmod", fmod)
PMATH_FUNC2("hypot", hypot)
PMATH_FUNC2("pow", pow)
PMATH_FUNC1("sin", sin)
PMATH_FUNC1("sinh", sinh)
PMATH_FUNC1("sqrt", sqrt)
PMATH_FUNC1("tan", tan)
PMATH_FUNC1("tanh", tanh)

INITIALIZATIONFN
void psyco_initmath(void)
{
    PyObject* md = Psyco_DefineMetaModule("math");

#if MATHMODULE_USES_METH_O    /* Python >= 2.6 */
#  define METH1 METH_O
#else
#  define METH1 METH_VARARGS
#endif

    fallback_acos = Psyco_DefineModuleFn(md, "acos", METH1,        pmath_acos);
    fallback_asin = Psyco_DefineModuleFn(md, "asin", METH1,        pmath_asin);
    fallback_atan = Psyco_DefineModuleFn(md, "atan", METH1,        pmath_atan);
    fallback_atan2= Psyco_DefineModuleFn(md, "atan2",METH_VARARGS, pmath_atan2);
    fallback_ceil = Psyco_DefineModuleFn(md, "ceil", METH1,        pmath_ceil);
    fallback_cos  = Psyco_DefineModuleFn(md, "cos",  METH1,        pmath_cos);
    fallback_cosh = Psyco_DefineModuleFn(md, "cosh", METH1,        pmath_cosh);
    fallback_exp  = Psyco_DefineModuleFn(md, "exp",  METH1,        pmath_exp);
    fallback_fabs = Psyco_DefineModuleFn(md, "fabs", METH1,        pmath_fabs);
    fallback_floor= Psyco_DefineModuleFn(md, "floor",METH1,        pmath_floor);
    fallback_fmod = Psyco_DefineModuleFn(md, "fmod", METH_VARARGS, pmath_fmod);
    fallback_hypot= Psyco_DefineModuleFn(md, "hypot",METH_VARARGS, pmath_hypot);
    fallback_pow  = Psyco_DefineModuleFn(md, "pow",  METH_VARARGS, pmath_pow);
    fallback_sin  = Psyco_DefineModuleFn(md, "sin",  METH1,        pmath_sin);
    fallback_sinh = Psyco_DefineModuleFn(md, "sinh", METH1,        pmath_sinh);
    fallback_sqrt = Psyco_DefineModuleFn(md, "sqrt", METH1,        pmath_sqrt);
    fallback_tan  = Psyco_DefineModuleFn(md, "tan",  METH1,        pmath_tan);
    fallback_tanh = Psyco_DefineModuleFn(md, "tanh", METH1,        pmath_tanh);

#undef METH1

    Py_XDECREF(md);
}

#else /* !HAVE_FP_FN_CALLS */
INITIALIZATIONFN
void psyco_initmath(void)
{
}
#endif /* !HAVE_FP_FN_CALLS */
