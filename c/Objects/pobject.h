 /***************************************************************/
/***             Psyco equivalent of object.h                  ***/
 /***************************************************************/

#ifndef _PSY_OBJECT_H
#define _PSY_OBJECT_H


#include "../Python/pycompiler.h"


#define OB_REFCOUNT         QUARTER(offsetof(PyObject, ob_refcnt))
#define OB_TYPE             QUARTER(offsetof(PyObject, ob_type))
#define VAR_OB_SIZE         QUARTER(offsetof(PyVarObject, ob_size))


/* common type checkers, rewritten because in Psyco we manipulate type
   objects directly and Python's usual macros insist on taking a regular
   PyObject* whose type is checked. */
#if NEW_STYLE_TYPES
# define PyType_TypeCheck(tp1, tp)  	\
	((tp1) == (tp) || PyType_IsSubtype((tp1), (tp)))
#else
# define PyType_TypeCheck(tp1, tp)    ((tp1) == (tp))
#endif

#define PsycoIter_Check(tp) \
    (PyType_HasFeature(tp, Py_TPFLAGS_HAVE_ITER) && \
     (tp)->tp_iternext != NULL)

#define PsycoSequence_Check(tp) \
	((tp)->tp_as_sequence && (tp)->tp_as_sequence->sq_item != NULL)


/* Return the type of an object, or NULL in case of exception (typically
   a promotion exception). */
inline PyTypeObject* Psyco_NeedType(PsycoObject* po, vinfo_t* vi) {
	 vinfo_t* vtp = get_array_item(po, vi, OB_TYPE);
	 if (vtp == NULL)
		 return NULL;
	 return (PyTypeObject*) psyco_pyobj_atcompiletime(po, vtp);
}
inline PyTypeObject* Psyco_FastType(vinfo_t* vi) {
	/* only call this when you know the type has already been
	   loaded by a previous Psyco_NeedType() */
	vinfo_t* vtp = vinfo_getitem(vi, OB_TYPE);
	extra_assert(vtp != NULL && is_compiletime(vtp->source));
	return (PyTypeObject*) (CompileTime_Get(vtp->source)->value);
}
/* Check for the type of an object. Returns the index in the set 'fs' or
   -1 if not in the set (or if exception). Used this is better than
   Psyco_NeedType() if you are only interested in some types, not all of them. */
inline int Psyco_TypeSwitch(PsycoObject* po, vinfo_t* vi, fixed_switch_t* fs) {
	vinfo_t* vtp = get_array_item(po, vi, OB_TYPE);
	 if (vtp == NULL)
		 return -1;
	 return psyco_switch_index(po, vtp, fs);
}

/* Use this to assert the type of an object. Do not use unless you are
   sure about it! (e.g. don't use this for integer-computing functions
   if they might return a long in case of overflow) */
inline void Psyco_AssertType(PsycoObject* po, vinfo_t* vi, PyTypeObject* tp) {
	vinfo_setitem(po, vi, OB_TYPE,
		      vinfo_new(CompileTime_New((long) tp)));
}


/* The following functions are Psyco implementations of common functions
   of the standard interpreter. */
EXTERNFN vinfo_t* PsycoObject_IsTrue(PsycoObject* po, vinfo_t* vi);
EXTERNFN vinfo_t* PsycoObject_Repr(PsycoObject* po, vinfo_t* vi);

/* Note: DelAttr() is SetAttr() with 'v' set to NULL (and not some vinfo_t
   that would happend to contain zero). */
EXTERNFN vinfo_t* PsycoObject_GetAttr(PsycoObject* po, vinfo_t* o,
                                      vinfo_t* attr_name);
EXTERNFN bool PsycoObject_SetAttr(PsycoObject* po, vinfo_t* o,
                                  vinfo_t* attr_name, vinfo_t* v);
EXTERNFN vinfo_t* PsycoObject_GenericGetAttr(PsycoObject* po, vinfo_t* obj,
                                             vinfo_t* vname);

EXTERNFN vinfo_t* PsycoObject_RichCompare(PsycoObject* po, vinfo_t* v,
					  vinfo_t* w, int op);
EXTERNFN vinfo_t* PsycoObject_RichCompareBool(PsycoObject* po,
                                              vinfo_t* v,
                                              vinfo_t* w, int op);


/* a quick way to specify the type of the object returned by an operation
   when it is known, without having to go into all the details of the
   operation itself (be careful, you must be *sure* of the return type): */

#define DEF_KNOWN_RET_TYPE_1(cname, op, flags, knowntype)	\
    DEF_KNOWN_RET_TYPE_internal(cname, knowntype,		\
				(PsycoObject* po, vinfo_t* v1),	\
				(po, op, flags, "v", v1))
#define DEF_KNOWN_RET_TYPE_2(cname, op, flags, knowntype)			\
    DEF_KNOWN_RET_TYPE_internal(cname, knowntype,				\
				(PsycoObject* po, vinfo_t* v1, vinfo_t* v2),	\
				(po, op, flags, "vv", v1, v2))
#define DEF_KNOWN_RET_TYPE_3(cname, op, flags, knowntype)			\
    DEF_KNOWN_RET_TYPE_internal(cname, knowntype,				\
				(PsycoObject* po, vinfo_t* v1, vinfo_t* v2,	\
					vinfo_t* v3),				\
				(po, op, flags, "vvv", v1, v2, v3))
#define DEF_KNOWN_RET_TYPE_4(cname, op, flags, knowntype)			\
    DEF_KNOWN_RET_TYPE_internal(cname, knowntype,				\
				(PsycoObject* po, vinfo_t* v1, vinfo_t* v2,	\
					vinfo_t* v3, vinfo_t* v4),		\
				(po, op, flags, "vvvv", v1, v2, v3, v4))

#define DEF_KNOWN_RET_TYPE_internal(cname, knowntype, fargs, gargs)             \
static vinfo_t* cname  fargs  {                                                 \
	vinfo_t* result = psyco_generic_call  gargs ;                           \
	if (result != NULL) {                                                   \
		set_array_item(po, result, OB_TYPE,                             \
			       vinfo_new(CompileTime_New((long)(knowntype))));  \
	}                                                                       \
	return result;                                                          \
}


/* definition of commonly used "fixed switches", i.e. sets of
   values (duplicating fixed switch structures for the same set
   of value can inccur a run-time performance loss in some cases) */

/* the variable names list the values in order.
   'int' means '&PyInt_Type' etc. */
EXTERNVAR fixed_switch_t psyfs_int;
EXTERNVAR fixed_switch_t psyfs_int_long;
EXTERNVAR fixed_switch_t psyfs_int_long_float;
EXTERNVAR fixed_switch_t psyfs_tuple_list;
EXTERNVAR fixed_switch_t psyfs_string_unicode;
EXTERNVAR fixed_switch_t psyfs_tuple;
EXTERNVAR fixed_switch_t psyfs_dict;
EXTERNVAR fixed_switch_t psyfs_none;
/* NOTE: don't forget to update pobject.c when adding new variables here */


/* To check whether an object is Py_None. Return 0 if it is,
   and -1 otherwise (use PycException_Occurred() to check for exceptions) */
inline int Psyco_IsNone(PsycoObject* po, vinfo_t* vi) {
	return Psyco_TypeSwitch(po, vi, &psyfs_none);
}


#endif /* _PSY_OBJECT_H */
