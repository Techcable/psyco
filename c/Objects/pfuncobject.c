#include "pfuncobject.h"
#include "pclassobject.h"


 /***************************************************************/
  /***   Virtual functions                                     ***/

#define FUNC_CODE      QUARTER(offsetof(PyFunctionObject, func_code))
#define FUNC_GLOBALS   QUARTER(offsetof(PyFunctionObject, func_globals))
#define FUNC_DEFAULTS  QUARTER(offsetof(PyFunctionObject, func_defaults))

static source_virtual_t psyco_computed_function;

static bool compute_function(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* newobj;
	vinfo_t* fcode;
	vinfo_t* fglobals;
	vinfo_t* fdefaults;

	fcode = vinfo_getitem(v, FUNC_CODE);
	if (fcode == NULL)
		return false;

	fglobals = vinfo_getitem(v, FUNC_GLOBALS);
	if (fglobals == NULL)
		return false;

	fdefaults = vinfo_getitem(v, FUNC_DEFAULTS);
	if (fdefaults == NULL)
		return false;

	newobj = psyco_generic_call(po, PyFunction_New,
				    CfReturnRef|CfPyErrIfNull,
				    "vv", fcode, fglobals);
	if (newobj == NULL)
		return false;

	if (!psyco_knowntobe(fdefaults, (long) NULL)) {
		if (!psyco_generic_call(po, PyFunction_SetDefaults,
					CfNoReturnValue|CfPyErrIfNonNull,
					"vv", newobj, fdefaults))
			return false;
	}
	
	/* move the resulting non-virtual Python object back into 'v' */
	vinfo_move(po, v, newobj);
	return true;
}


DEFINEFN
vinfo_t* PsycoFunction_New(PsycoObject* po, vinfo_t* fcode,
			   vinfo_t* fglobals, vinfo_t* fdefaults)
{
	vinfo_t* r = vinfo_new(VirtualTime_New(&psyco_computed_function));
	r->array = array_new(MAX3(FUNC_CODE, FUNC_GLOBALS, FUNC_DEFAULTS)+1);
	r->array->items[OB_TYPE] =
		vinfo_new(CompileTime_New((long)(&PyFunction_Type)));
	vinfo_incref(fcode);
	r->array->items[FUNC_CODE] = fcode;
	vinfo_incref(fglobals);
	r->array->items[FUNC_GLOBALS] = fglobals;
	if (fdefaults == NULL)
		fdefaults = psyco_vi_Zero();
	else
		vinfo_incref(fdefaults);
	r->array->items[FUNC_DEFAULTS] = fdefaults;
	return r;
}


 /***************************************************************/
  /*** function objects meta-implementation                    ***/

DEFINEFN
vinfo_t* pfunction_call(PsycoObject* po, vinfo_t* func,
                        vinfo_t* arg, vinfo_t* kw)
{
	if (psyco_knowntobe(kw, (long) NULL)) {
		PyCodeObject* co;
		vinfo_t* fcode;
		vinfo_t* fglobals;
		vinfo_t* fdefaults;

		if (!is_virtualtime(func->source)) {
			/* run-time or compile-time values: promote the
			   function object as a whole into compile-time */
			vinfo_t* result;
			PyObject* f = psyco_pyobj_atcompiletime(po, func);
			PyObject* glob;
			PyObject* defl;
			if (f == NULL)
				return NULL;

			co = (PyCodeObject*) PyFunction_GET_CODE(f);
			glob = PyFunction_GET_GLOBALS(f);
			defl = PyFunction_GET_DEFAULTS(f);
			Py_INCREF(glob);
			fglobals = vinfo_new(CompileTime_NewSk(sk_new
					     ((long)glob, SkFlagPyObj)));
			if (defl == NULL)
				fdefaults = psyco_vi_Zero();
			else {
				Py_INCREF(defl);
				fdefaults = vinfo_new(CompileTime_NewSk(sk_new
						     ((long)defl, SkFlagPyObj)));
			}
			
			result = psyco_call_pyfunc(po, co, fglobals, fdefaults,
						 arg, po->pr.auto_recursion);
			vinfo_decref(fdefaults, po);
			vinfo_decref(fglobals, po);
			return result;
		}
		else {
			/* virtual-time function objects: read the
			   individual components */
			fcode = vinfo_getitem(func, FUNC_CODE);
			if (fcode == NULL)
				return NULL;
			co = (PyCodeObject*)psyco_pyobj_atcompiletime(po, fcode);
			if (co == NULL)
				return NULL;
			
			fglobals = vinfo_getitem(func, FUNC_GLOBALS);
			if (fglobals == NULL)
				return NULL;
			
			fdefaults = vinfo_getitem(func, FUNC_DEFAULTS);
			if (fdefaults == NULL)
				return NULL;

			return psyco_call_pyfunc(po, co, fglobals, fdefaults,
						 arg, po->pr.auto_recursion);
		}
	}
	else {
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
		return psyco_generic_call(po, PyFunction_Type.tp_call,
					  CfReturnRef|CfPyErrIfNull,
					  "vvv", func, arg, kw);
#else
		/* PyFunction_Type.tp_call == NULL... */
		return psyco_generic_call(po, PyEval_CallObjectWithKeywords,
					  CfReturnRef|CfPyErrIfNull,
					  "vvv", func, arg, kw);
#endif
	}
}


#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
static vinfo_t* pfunc_descr_get(PsycoObject* po, PyObject* func,
				vinfo_t* obj, PyObject* type)
{
	/* see comments of pmember_get() in pdescrobject.c. */

	/* XXX obj is never Py_None here in the current implementation,
	   but could be if called by other routines than
	   PsycoObject_GenericGetAttr(). */
	return PsycoMethod_New(func, obj, type);
}
#endif /* NEW_STYLE_TYPES */


INITIALIZATIONFN
void psy_funcobject_init(void)
{
#if NEW_STYLE_TYPES   /* Python >= 2.2b1 */
	Psyco_DefineMeta(PyFunction_Type.tp_call, pfunction_call);
	Psyco_DefineMeta(PyFunction_Type.tp_descr_get, pfunc_descr_get);
#endif
	psyco_computed_function.compute_fn = &compute_function;
}
