#include "pintobject.h"


DEFINEFN
vinfo_t* PsycoInt_AsLong(PsycoObject* po, vinfo_t* v)
{
	vinfo_t* result;
	PyNumberMethods *nb;
	PyTypeObject* tp;
	
	tp = Psyco_NeedType(po, v);
	if (tp == NULL)
		return NULL;

	if (PsycoInt_Check(tp)) {
		result = PsycoInt_AS_LONG(po, v);
		vinfo_incref(result);
		return result;
	}

	if ((nb = tp->tp_as_number) == NULL || nb->nb_int == NULL) {
		PycException_SetString(po, PyExc_TypeError,
				       "an integer is required");
		return NULL;
	}

	v = Psyco_META1(po, nb->nb_int,
			CfReturnRef|CfPyErrIfNull,
			"v", v);
	if (v == NULL)
		return NULL;
	
	/* silently assumes the result is an integer object */
	result = PsycoInt_AS_LONG(po, v);
	vinfo_incref(result);
	vinfo_decref(v, po);
	return result;
}

static bool compute_int(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* newobj;
	vinfo_t* x;
	
	/* get the field 'ob_ival' from the Python object 'intobj' */
	x = get_array_item(po, intobj, INT_OB_IVAL);
	if (x == NULL)
		return false;

	/* call PyInt_FromLong() */
	newobj = psyco_generic_call(po, PyInt_FromLong,
				    CfPure|CfReturnRef|CfPyErrIfNull, "v", x);
	if (newobj == NULL)
		return false;

	/* move the resulting non-virtual Python object back into 'intobj' */
	vinfo_move(po, intobj, newobj);
	return true;
}


DEFINEVAR source_virtual_t psyco_computed_int;


 /***************************************************************/
  /*** integer objects meta-implementation                     ***/

static condition_code_t pint_nonzero(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	return integer_non_null(po, ival);
}

static vinfo_t* pint_pos(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* vtp = vinfo_getitem(intobj, OB_TYPE);  /* might be NULL */
	if (psyco_knowntobe(vtp, (long)(&PyInt_Type))) {
		vinfo_incref(intobj);
		return intobj;
	}
	else {
		vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
		if (ival == NULL)
			return NULL;
		return PsycoInt_FromLong(ival);
	}
}

static vinfo_t* pint_neg(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* result;
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	if (ival == NULL)
		return NULL;
	result = integer_neg(po, ival, true);
	if (result != NULL)
		return PsycoInt_FROM_LONG(result);
	
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_negative,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "v", intobj);
}

static vinfo_t* pint_invert(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* result;
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	if (ival == NULL)
		return NULL;
	result = integer_not(po, ival);
	if (result != NULL)
		result = PsycoInt_FROM_LONG(result);
	return result;
}

static vinfo_t* pint_abs(PsycoObject* po, vinfo_t* intobj)
{
	vinfo_t* result;
	vinfo_t* ival = PsycoInt_AS_LONG(po, intobj);
	if (ival == NULL)
		return NULL;
	result = integer_abs(po, ival, true);
	if (result != NULL)
		return PsycoInt_FROM_LONG(result);
	
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_absolute,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "v", intobj);
}


#define CONVERT_TO_LONG(vobj, vlng)				\
	if (Psyco_TypeSwitch(po, vobj, &psyfs_int) == 0) {	\
		vlng = PsycoInt_AS_LONG(po, vobj);		\
		if (vlng == NULL)				\
			return NULL;				\
	}							\
	else {							\
		if (PycException_Occurred(po))			\
			return NULL;				\
		vinfo_incref(psyco_viNotImplemented);		\
		return psyco_viNotImplemented;			\
	}

static vinfo_t* pint_add(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_add(po, a, b, true);
	if (x != NULL)
		return PsycoInt_FROM_LONG(x);
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_add,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "vv", v, w);
}

static vinfo_t* pint_sub(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_sub(po, a, b, true);
	if (x != NULL)
		return PsycoInt_FROM_LONG(x);
	if (PycException_Occurred(po))
		return NULL;
	/* overflow */
	return psyco_generic_call(po, PyInt_Type.tp_as_number->nb_subtract,
				  CfPure|CfReturnRef|CfPyErrIfNull,
				  "vv", v, w);
}

static vinfo_t* pint_or(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_or(po, a, b);
	if (x != NULL)
		x = PsycoInt_FROM_LONG(x);
	return x;
}

static vinfo_t* pint_and(PsycoObject* po, vinfo_t* v, vinfo_t* w)
{
	vinfo_t* a;
	vinfo_t* b;
	vinfo_t* x;
	CONVERT_TO_LONG(v, a);
	CONVERT_TO_LONG(w, b);
	x = integer_and(po, a, b);
	if (x != NULL)
		x = PsycoInt_FROM_LONG(x);
	return x;
}


DEFINEFN
void psy_intobject_init()
{
	PyNumberMethods *m = PyInt_Type.tp_as_number;
	Psyco_DefineMeta(m->nb_nonzero,  pint_nonzero);
	
	Psyco_DefineMeta(m->nb_positive, pint_pos);
	Psyco_DefineMeta(m->nb_negative, pint_neg);
	Psyco_DefineMeta(m->nb_invert,   pint_invert);
	Psyco_DefineMeta(m->nb_absolute, pint_abs);
	
	Psyco_DefineMeta(m->nb_add,      pint_add);
	Psyco_DefineMeta(m->nb_subtract, pint_sub);
	Psyco_DefineMeta(m->nb_or,       pint_or);
	Psyco_DefineMeta(m->nb_and,      pint_and);

	psyco_computed_int.compute_fn = &compute_int;
}
