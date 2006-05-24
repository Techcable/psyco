#include "pbltinmodule.h"
#include "../Objects/pintobject.h"
#include "../Objects/ptupleobject.h"
#include "../Objects/plistobject.h"
#include "../Objects/pstringobject.h"
#include "../Objects/prangeobject.h"


static PyCFunction cimpl_range;
static PyCFunction cimpl_xrange;
static PyCFunction cimpl_chr;
static PyCFunction cimpl_ord;
static PyCFunction cimpl_id;
static PyCFunction cimpl_hash;
static PyCFunction cimpl_min;
static PyCFunction cimpl_max;
static PyCFunction cimpl_sum;
static PyCFunction cimpl_len;
static PyCFunction cimpl_abs;
static PyCFunction cimpl_apply;
static PyCFunction cimpl_divmod;


#if HAVE_METH_O
# define METH_O_WRAPPER(name, self, arg)    do { } while (0)  /* nothing */
#else
# define METH_O_WRAPPER(name, self, arg)    do {                                \
    if (PsycoTuple_Load(arg) != 1)                                              \
      return psyco_generic_call(po, cimpl_ ## name, CfReturnRef|CfPyErrIfNull,  \
                                "vv", self, arg);                               \
    arg = PsycoTuple_GET_ITEM(arg, 0);                                          \
} while (0)
#endif

#if HAVE_METH_NOARGS
# define METH_NOARGS_WRAPPER(name, self, arg)  do { } while (0)  /* nothing */
#else
# define METH_NOARGS_WRAPPER(name, self, arg)  do {                             \
    if (PsycoTuple_Load(arg) != 0)                                              \
      return psyco_generic_call(po, cimpl_ ## name, CfReturnRef|CfPyErrIfNull,  \
                                "vv", self, arg);                               \
} while (0)
#endif


static vinfo_t* get_len_of_range(PsycoObject* po, vinfo_t* lo, vinfo_t* hi
				 /*, vinfo_t* step == 1 currently*/)
{
	/* translated from bltinmodule.c */
	condition_code_t cc = integer_cmp(po, lo, hi, Py_LT);
	if (cc == CC_ERROR)
		return NULL;
	if (runtime_condition_t(po, cc)) {
		vinfo_t* vresult = integer_sub(po, hi, lo, false);
		assert_nonneg(vresult);
		return vresult;
	}
	else
		return psyco_vi_Zero();
}

static vinfo_t* intobj_as_long(PsycoObject* po, vinfo_t* v)
{
	if (Psyco_VerifyType(po, v, &PyInt_Type) == 1)
		return PsycoInt_AS_LONG(po, v);
	else
		return NULL;
}

static bool parse_range_args(PsycoObject* po, vinfo_t* vargs,
			     vinfo_t** iistart, vinfo_t** iilen)
{
	vinfo_t* ilow;
	vinfo_t* ihigh;
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	
	switch (tuplesize) {
	case 1:
		ihigh = intobj_as_long(po, PsycoTuple_GET_ITEM(vargs, 0));
		if (ihigh == NULL) return false;
		ilow = psyco_vi_Zero();
		vinfo_incref(ihigh);
		break;
	/*case 3:
		istep = intobj_as_long(po, PsycoTuple_GET_ITEM(vargs, 2));
		if (istep == NULL) return NULL;*/
		/* fall through */
	case 2:
		ilow  = intobj_as_long(po, PsycoTuple_GET_ITEM(vargs, 0));
		if (ilow == NULL) return false;
		ihigh = intobj_as_long(po, PsycoTuple_GET_ITEM(vargs, 1));
		if (ihigh == NULL) return false;
		vinfo_incref(ilow);
		vinfo_incref(ihigh);
		break;
	default:
		return false;
	}
	*iilen = get_len_of_range(po, ilow, ihigh);
	vinfo_decref(ihigh, po);
	if (*iilen == NULL) {
		vinfo_decref(ilow, po);
		return false;
	}
	*iistart = ilow;
	return true;
}

static vinfo_t* pbuiltin_range(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* istart;
	vinfo_t* ilen;
	if (parse_range_args(po, vargs, &istart, &ilen)) {
		return PsycoListRange_NEW(po, istart, ilen);
	}
	if (PycException_Occurred(po))
		return NULL;
	return psyco_generic_call(po, cimpl_range,
				  CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

static vinfo_t* pbuiltin_xrange(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* istart;
	vinfo_t* ilen;
	if (parse_range_args(po, vargs, &istart, &ilen)) {
		return PsycoXRange_NEW(po, istart, ilen);
	}
	if (PycException_Occurred(po))
		return NULL;
	return psyco_generic_call(po, cimpl_xrange,
				  CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

#if NEW_STYLE_TYPES
static vinfo_t* prange_new(PsycoObject* po, PyTypeObject* type,
			   vinfo_t* vargs, vinfo_t* vkw)
{
	/* for Python >= 2.3, where __builtin__.xrange is a type */
	vinfo_t* istart;
	vinfo_t* ilen;
	psyco_assert(type == &PyRange_Type);   /* no subclassing xrange */
	if (parse_range_args(po, vargs, &istart, &ilen)) {
		return PsycoXRange_NEW(po, istart, ilen);
	}
	if (PycException_Occurred(po))
		return NULL;
	return psyco_generic_call(po, type->tp_new,
				  CfReturnRef|CfPyErrIfNull,
				  "lvv", type, vargs, vkw);
}
#else
# define prange_new  NULL
#endif

static vinfo_t* pbuiltin_chr(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* intval;
	vinfo_t* result;
	condition_code_t cc;
	
	if (PsycoTuple_Load(vargs) != 1)
		goto use_proxy;
	intval = PsycoInt_AsLong(po, PsycoTuple_GET_ITEM(vargs, 0));
	if (intval == NULL)
		return NULL;

	cc = integer_cmp_i(po, intval, 255, Py_GT|COMPARE_UNSIGNED);
	if (cc == CC_ERROR) {
		vinfo_decref(intval, po);
		return NULL;
	}
	if (runtime_condition_f(po, cc)) {
		vinfo_decref(intval, po);
		goto use_proxy;
	}

	result = PsycoCharacter_New(intval);
	vinfo_decref(intval, po);
	return result;

   use_proxy:
	return psyco_generic_call(po, cimpl_chr, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

static vinfo_t* pbuiltin_ord(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	vinfo_t* result;
	METH_O_WRAPPER(ord, vself, vobj);

	if (!PsycoCharacter_Ord(po, vobj, &result))
		return NULL;
	
	if (result != NULL)
		return PsycoInt_FROM_LONG(result);
	
	return psyco_generic_call(po, cimpl_ord, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vobj);
}

static vinfo_t* pbuiltin_id(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	METH_O_WRAPPER(id, vself, vobj);
	return PsycoInt_FromLong(vobj);
}

static vinfo_t* pbuiltin_hash(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	vinfo_t* result;
	METH_O_WRAPPER(hash, vself, vobj);
	
	result = PsycoObject_Hash(po, vobj);
	if (result != NULL)
		result = PsycoInt_FROM_LONG(result);
	return result;
}

static vinfo_t* pbuiltin_len(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	vinfo_t* result;
	METH_O_WRAPPER(len, vself, vobj);
	
	result = PsycoObject_Size(po, vobj);
	if (result != NULL)
		result = PsycoInt_FROM_LONG(result);
	return result;
}

static vinfo_t* pbuiltin_abs(PsycoObject* po, vinfo_t* vself, vinfo_t* vobj)
{
	METH_O_WRAPPER(abs, vself, vobj);
	return PsycoNumber_Absolute(po, vobj);
}

static vinfo_t* pbuiltin_apply(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* alist = NULL;
	vinfo_t* kwdict = NULL;
	vinfo_t* retval;
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	PyTypeObject* argt;
	vinfo_t* t = NULL;

	switch (tuplesize) {
	case 3:
		kwdict = PsycoTuple_GET_ITEM(vargs, 2);
		if (Psyco_VerifyType(po, kwdict, &PyDict_Type) != true) {
			/* 'kwdict' is not a dictionary */
			break;
		}
		/* fall through */
	case 2:
		alist = PsycoTuple_GET_ITEM(vargs, 1);
		argt = Psyco_NeedType(po, alist);
		if (argt == NULL)
			return NULL;
		if (!PyType_TypeCheck(argt, &PyTuple_Type)) {
			/* 'alist' is not a tuple */
			if (!PsycoSequence_Check(argt))
				break;  /* give up */
			t = PsycoSequence_Tuple(po, alist);
			if (t == NULL)
				break;  /* give up */
			alist = t;
		}
		/* fall through */
	case 1:
		retval = PsycoEval_CallObjectWithKeywords(po,
					PsycoTuple_GET_ITEM(vargs, 0),
					alist, kwdict);
		vinfo_xdecref(t, po);
		return retval;
	}

	if (PycException_Occurred(po))
		return NULL;
	return psyco_generic_call(po, cimpl_apply, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

static vinfo_t* pbuiltin_divmod(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	
	if (tuplesize == 2) {
		return PsycoNumber_Divmod(po,
					  PsycoTuple_GET_ITEM(vargs, 0),
					  PsycoTuple_GET_ITEM(vargs, 1));
	}
	return psyco_generic_call(po, cimpl_divmod, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}


// XXX: min(), max(), cmp(), list(), set(), dict(), round()
// XXX: 64 bit
// XXX: kwargs?

static vinfo_t* pbuiltin_min_max(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs, PyCFunction cimpl, int op) 
{
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */

        // XXX: 1 arg: to iter over

	vinfo_t* item;
	vinfo_t* maxitem = NULL;
        vinfo_t* result;
	int i = 0;
	while (i < tuplesize) {
	    item = PsycoTuple_GET_ITEM(vargs, i);
		
		if (Psyco_VerifyType(po, item, &PyDict_Type)) {
			goto fail;
		}
		
	    if (maxitem == NULL) {
			maxitem = item;
		} else {
			result = PsycoObject_RichCompareBool(po, item, maxitem, op);
			if (result == NULL)
			   return NULL;
			   
			switch (runtime_NON_NULL_f(po, result)) {
				case true:
					maxitem = item;
					break;
				case false:
					break;
				default:
					goto fail;
			}
		}
		
		i++;
	}
	
	if (maxitem != NULL) {
	    vinfo_incref(maxitem);
	    return maxitem;
	}

fail:
	return psyco_generic_call(po, cimpl, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}

static vinfo_t* pbuiltin_min(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	return pbuiltin_min_max(po, vself, vargs, cimpl_min, Py_LT);
}

static vinfo_t* pbuiltin_max(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
    return pbuiltin_min_max(po, vself, vargs, cimpl_max, Py_GT);
}

static vinfo_t* pbuiltin_sum(PsycoObject* po, vinfo_t* vself, vinfo_t* vargs)
{
	vinfo_t* seq;
	vinfo_t* result = NULL;
	vinfo_t* temp;
	vinfo_t* item;
	vinfo_t* iter;
	
	int tuplesize = PsycoTuple_Load(vargs);  /* -1 if unknown */
	
	if (tuplesize < 1 || tuplesize > 2)
	    goto fail;
		
	seq = PsycoTuple_GET_ITEM(vargs, 0);
	
	printf("tuplesize=%d seq=%d\n", tuplesize, seq);
	
	iter = PsycoObject_GetIter(po, seq);
//	if (iter == NULL) {
//        printf("no iter!\n");
//		if (PycException_Occurred(po)) {
//		    PycException_Clear(po);
//		}
//	    goto fail;
//	}
		
	if (tuplesize > 1)
	    printf("tuplesize > 1\n");
	    result = PsycoTuple_GET_ITEM(vargs, 1);
		
	if (result != NULL) {
	    printf("Temporary early exity\n");
	    vinfo_incref(result);
	    return result;
	}

	goto fail;
			
	if (result == NULL) {
            printf("Offset from zero...\n");
    	    return psyco_generic_call(po, PyInt_AsLong,
				  CfReturnRef|CfPyErrCheckMinus1,
				  "l", 0);
	} else {	    
		if (Psyco_VerifyType(po, item, &PyBaseString_Type)) {
		    goto fail;
		}
	}
	
	vinfo_incref(result);
	return result;
	
	for(;;) {
	        item = PsycoIter_Next(po, iter);
		if (item == NULL) {
			extra_assert(PycException_Occurred(po));
			/* catch PyExc_StopIteration */
			temp = PycException_Matches(po, PyExc_StopIteration);
			if (runtime_NON_NULL_t(po, temp) == true) {
				/* iterator ended normally */
				PycException_Clear(po);
				break;
			} else {
			    printf("Exception!\n");
				goto fail;
			}
                }
		temp = PsycoNumber_Add(po, result, item);
		result = temp;
		if (result == NULL) {
		    goto fail;
		}
	}
	
	if (result != NULL) 
    	    vinfo_incref(result);
	    return result;

fail:
        printf("fail!\n");
        return psyco_generic_call(po, cimpl_sum, CfReturnRef|CfPyErrIfNull,
				  "lv", NULL, vargs);
}



/***************************************************************/


INITIALIZATIONFN
void psyco_bltinmodule_init(void)
{
	PyObject* md = Psyco_DefineMetaModule("__builtin__");

#define DEFMETA(name, flags)							\
    cimpl_ ## name = Psyco_DefineModuleFn(md, #name, flags, &pbuiltin_ ## name)

	DEFMETA( range,		METH_VARARGS );
	DEFMETA( chr,		METH_VARARGS );
	DEFMETA( ord,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( id,		HAVE_METH_O ? METH_O : METH_VARARGS );
        DEFMETA( hash,          HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( min,           METH_VARARGS );
	DEFMETA( max,           METH_VARARGS );
	DEFMETA( sum,           METH_VARARGS );
	DEFMETA( len,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( abs,		HAVE_METH_O ? METH_O : METH_VARARGS );
	DEFMETA( apply,		METH_VARARGS );
	DEFMETA( divmod,	METH_VARARGS );
	cimpl_xrange = Psyco_DefineModuleC(md, "xrange", METH_VARARGS,
                                           &pbuiltin_xrange, prange_new);
										   
#undef DEFMETA
	Py_XDECREF(md);
}
