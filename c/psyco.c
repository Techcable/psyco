#include "psyco.h"
#include "codemanager.h"
#include "vcompiler.h"
#include "dispatcher.h"
#include "processor.h"
#include "mergepoints.h"
#include "selective.h"
#include "Python/pycompiler.h"
#include "Objects/pobject.h"
#include "Objects/ptupleobject.h"

#include "initialize.h"  /* generated by files.py */


 /***************************************************************/
/***   Frame and arguments building                            ***/
 /***************************************************************/

DEFINEVAR PyObject* PyExc_PsycoError;

static void fix_run_time_args(PsycoObject * po, vinfo_array_t* target,
                              vinfo_array_t* source, RunTimeSource* sources)
{
  int i = source->count;
  extra_assert(i == target->count);
  while (i--)
    {
      vinfo_t* a = source->items[i];
      if (a != NULL && a->tmp != NULL)
        {
          vinfo_t* b = a->tmp;
          if (is_runtime(a->source))
            {
              if (target->items[i] == NULL)
                continue;  /* item was removed by psyco_simplify_array() */
              if (sources != NULL) {
                int argc = (po->stack_depth-INITIAL_STACK_DEPTH) / sizeof(long);
                sources[argc] = a->source;
              }
              po->stack_depth += sizeof(long);
              /* arguments get borrowed references */
              b->source = RunTime_NewStack(po->stack_depth, REG_NONE,
                                                false);
            }
          extra_assert(b == target->items[i]);
          a->tmp = NULL;
          if (a->array != NullArray)
            fix_run_time_args(po, b->array, a->array, sources);
        }
    }
}

#define BF_UNSUPPORTED  ((PsycoObject*) -1)

/* Build a PsycoObject "frame" corresponding to the call of a Python
   function. Raise a Python exception and return NULL in case of failure.
   Return BF_UNSUPPORTED if the bytecode contains unsupported instructions.
   The 'arginfo' array gives the number of arguments as well as
   additional information about them. If 'sources!=NULL', it is set to an
   array of the sources of the values that must be pushed to make the call. */
static PsycoObject* psyco_build_frame(PyCodeObject* co, vinfo_t* vglobals,
                                      vinfo_t** argarray, int argcount,
                                      vinfo_t** defarray, int defcount,
                                      int recursion, RunTimeSource** sources)
{
  /* build a "frame" in a PsycoObject according to the given code object. */

  PyObject* merge_points = psyco_get_merge_points(co);
  PsycoObject* po;
  RunTimeSource* source1;
  vinfo_array_t* inputvinfos;
  vinfo_array_t* arraycopy;
  int extras, i, minargcnt, inputargs, rtcount, ncells, nfrees;
  vinfo_t** pp;

  if (merge_points == Py_None)
    return BF_UNSUPPORTED;
  
  ncells = PyTuple_GET_SIZE(co->co_cellvars);
  nfrees = PyTuple_GET_SIZE(co->co_freevars);
  if (co->co_flags & CO_VARKEYWORDS)
    {
      debug_printf(("psyco: unsupported: %s has a ** argument\n",
                    PyCodeObject_NAME(co)));
      return BF_UNSUPPORTED;
    }
  if (ncells != 0 || nfrees != 0)
    {
      debug_printf(("psyco: unsupported: %s has free or cell variables\n",
                    PyCodeObject_NAME(co)));
      return BF_UNSUPPORTED;
    }
  minargcnt = co->co_argcount - defcount;
  inputargs = argcount;
  if (inputargs != co->co_argcount)
    {
      if (inputargs > co->co_argcount && (co->co_flags & CO_VARARGS))
        /* ok, extra args will be collected below */ ;
      else
        {
          if (inputargs < minargcnt || inputargs > co->co_argcount)
            {
              int n = co->co_argcount < minargcnt ? minargcnt : co->co_argcount;
              PyErr_Format(PyExc_TypeError,
                           "%.200s() takes %s %d %sargument%s (%d given)",
                           PyCodeObject_NAME(co),
                           minargcnt == co->co_argcount ? "exactly" :
                           (inputargs < n ? "at least" : "at most"),
                           n,
                           /*kwcount ? "non-keyword " :*/ "",
                           n == 1 ? "" : "s",
                           inputargs);
              return NULL;
            }
          inputargs = co->co_argcount;  /* add default arguments */
        }
    }

  /* Collect all input vinfo_t's (globals, arguments, needed default values)
     into a new array that mimics the PsycoObject's vlocals. */
  inputvinfos = array_new(INDEX_LOC_LOCALS_PLUS + inputargs);
  inputvinfos->items[INDEX_LOC_GLOBALS] = vglobals;
  for (i=0; i<argcount; i++)
    inputvinfos->items[INDEX_LOC_LOCALS_PLUS+i] = argarray[i];
  for (; i<inputargs; i++)
    inputvinfos->items[INDEX_LOC_LOCALS_PLUS+i] = defarray[i-minargcnt];
  
  extras = co->co_stacksize + co->co_nlocals + ncells + nfrees;

  po = PsycoObject_New(INDEX_LOC_LOCALS_PLUS + extras);
  po->stack_depth = INITIAL_STACK_DEPTH;
  po->vlocals.count = INDEX_LOC_LOCALS_PLUS + extras;
  po->last_used_reg = REG_LOOP_START;
  po->pr.auto_recursion = recursion;

  /* duplicate the inputvinfos. If two arguments share some common part, they
     will also share it in the copy. */
  clear_tmp_marks(inputvinfos);
  arraycopy = array_new(inputvinfos->count);
  duplicate_array(arraycopy, inputvinfos);

  /* simplify arraycopy in the sense of psyco_simplify_array() */
  rtcount = psyco_simplify_array(arraycopy);

  /* all run-time arguments or argument parts must be corrected: in the
     input vinfo_t's they have arbitrary sources, but in the new frame's
     sources they will have to be fetched from the machine stack, where
     the caller will have pushed them. */
  if (sources != NULL)
    {
      source1 = PyCore_MALLOC(rtcount * sizeof(RunTimeSource));
      if (source1 == NULL && rtcount > 0)
        OUT_OF_MEMORY();
      *sources = source1;
    }
  else
    source1 = NULL;
  fix_run_time_args(po, arraycopy, inputvinfos, source1);
  array_release(inputvinfos);

  /* initialize po->vlocals */
  LOC_GLOBALS = arraycopy->items[INDEX_LOC_GLOBALS];

  /* move the arguments into their target place,
     excluding the ones that map to the '*' parameter */
  pp = arraycopy->items + INDEX_LOC_LOCALS_PLUS;
  for (i=0; i<co->co_argcount; i++)
    LOC_LOCALS_PLUS[i] = *pp++;
  if (co->co_flags & CO_VARARGS)
    {
      /* store the extra args in a virtual-time tuple */
      LOC_LOCALS_PLUS[i] = PsycoTuple_New(inputargs - i, pp);
      for (; inputargs > i; inputargs--)
        vinfo_decref(*pp++, NULL);
      i++;
    }
  else
    extra_assert(i == inputargs);
  array_release(arraycopy);

  /* the rest of locals is uninitialized */
  for (; i<co->co_nlocals; i++)
    LOC_LOCALS_PLUS[i] = psyco_vi_Zero();
  /* the rest of the array is the currently empty stack,
     set to NULL by array_new(). */
  
  /* store the code object */
  po->pr.co = co;
  Py_INCREF(co);  /* XXX never freed */
  pyc_data_build(po, merge_points);

  /* set up the CALL return address */
  po->stack_depth += sizeof(long);
  LOC_CONTINUATION = vinfo_new(RunTime_NewStack(po->stack_depth, REG_NONE,
                                                false));
  psyco_assert_coherent(po);
  return po;
}

static PyObject* cimpl_call_pyfunc(PyCodeObject* co, PyObject* globals,
                                   PyObject* defaults, PyObject* arg)
{
  /* simple wrapper around PyEval_EvalCodeEx, for the fail_to_default
     case of psyco_call_pyfunc() */
  
#if HAVE_PyEval_EvalCodeEx
  int defcount = (defaults ? PyTuple_GET_SIZE(defaults) : 0);
  PyObject** defs = (defcount ? &PyTuple_GET_ITEM(defaults, 0) : NULL);
  return PyEval_EvalCodeEx(co, globals, (PyObject*)NULL,
                           &PyTuple_GET_ITEM(arg, 0), PyTuple_GET_SIZE(arg),
                           (PyObject**)NULL, 0,
                           defs, defcount, NULL);
#else
  /* No PyEval_EvalCodeEx()! Dummy work-around. */
  PyObject* result = NULL;
  PyObject* hack = PyFunction_New((PyObject*) co, globals);
  if (hack != NULL)
    {
      if (defaults == NULL || !PyFunction_SetDefaults(hack, defaults))
        result = PyEval_CallObject(hack, arg);
      Py_DECREF(hack);
    }
  return result;
#endif
}

DEFINEFN
vinfo_t* psyco_call_pyfunc(PsycoObject* po, PyCodeObject* co,
                           vinfo_t* vglobals, vinfo_t* vdefaults,
                           vinfo_t* arg_tuple, int recursion)
{
  CodeBufferObject* codebuf;
  PsycoObject* mypo;
  Source* sources;
  vinfo_t* result;
  int tuple_size, argcount, defcount;
  condition_code_t cc;

  if (is_proxycode(co))
    co = ((PsycoFunctionObject*) PyTuple_GET_ITEM(co->co_consts, 1))->psy_code;
  
  tuple_size = PsycoTuple_Load(arg_tuple);
#if HAVE_PyEval_EvalCodeEx
  /* The following two lines are moved a bit further for Python < 2.2 */
  if (tuple_size == -1)
    goto fail_to_default;
      /* XXX calling with an unknown-at-compile-time number of arguments
         is not implemented, revert to the default behaviour */
#endif

  /* is vdefaults!=NULL at run-time ? */
  cc = object_non_null(po, vdefaults);
  if (cc == CC_ERROR)  /* error or more likely promotion */
    return NULL;
  if (runtime_condition_t(po, cc))
    {
      defcount = PsycoTuple_Load(vdefaults);
      if (defcount == -1)
        goto fail_to_default;
          /* calling with an unknown-at-compile-time number of default arguments
             is not implemented (but this is probably not useful to implement);
             revert to the default behaviour */
    }
  else
    defcount = 0;

#if !HAVE_PyEval_EvalCodeEx
  if (tuple_size == -1)
    goto fail_to_default;
#endif

  /* the processor condition codes will be messed up soon */
  BEGIN_CODE
  NEED_CC();
  END_CODE
  
  /* prepare a frame */
  mypo = psyco_build_frame(co, vglobals,
                           &PsycoTuple_GET_ITEM(arg_tuple, 0), tuple_size,
                           &PsycoTuple_GET_ITEM(vdefaults, 0), defcount,
                           recursion, &sources);
  if (mypo == BF_UNSUPPORTED)
    goto fail_to_default;
  if (mypo == NULL)
    goto pyerr;
  argcount = get_arguments_count(&mypo->vlocals);

  /* compile the function (this frees mypo) */
  codebuf = psyco_compile_code(mypo,
                               psyco_first_merge_point(mypo->pr.merge_points));

  /* get the run-time argument sources and push them on the stack
     and write the actual CALL */
  result = psyco_call_psyco(po, codebuf, sources, argcount);
  PyCore_FREE(sources);
  return result;

 fail_to_default:
  
#if !HAVE_PyEval_EvalCodeEx
  /* Big hack. Without PyEval_EvalCodeEx(), there is no way I am aware
     of to call an arbitrary code object with arguments apart from
     building a temporary function object around it. So we try to build
     it now if we have enough information to do so. */
  if (is_compiletime(vglobals->source))
    {
      int i;
      for (i=0; i<defcount; i++)
        if (!is_compiletime(PsycoTuple_GET_ITEM(vdefaults, i)->source))
          defcount = -1;
      
      if (defcount != -1)
        {
          PyObject* hack = PyFunction_New((PyObject*) co,
                         (PyObject*) CompileTime_Get(vglobals->source)->value);
          if (hack == NULL)
            goto pyerr;
          if (defcount > 0)
            {
              PyObject* defaults = PyTuple_New(defcount);
              if (defaults == NULL) {
                Py_DECREF(hack);
                goto pyerr;
              }
              for (i=0; i<defcount; i++)
                PyTuple_SET_ITEM(defaults, i, (PyObject*) CompileTime_Get(
                            PsycoTuple_GET_ITEM(vdefaults, i)->source)->value);
              i = PyFunction_SetDefaults(hack, defaults);
              Py_DECREF(defaults);
              if (i) {
                Py_DECREF(hack);
                goto pyerr;
              }
            }
          /* XXX lost ref to hack */
          return psyco_generic_call(po, PyEval_CallObjectWithKeywords,
                                    CfReturnRef|CfPyErrIfNull,
                                    "lvl", hack, arg_tuple, NULL);
        }
    }
#endif /* !HAVE_PyEval_EvalCodeEx */
    
  return psyco_generic_call(po, cimpl_call_pyfunc,
                            CfReturnRef|CfPyErrIfNull,
                            "lvvv", co, vglobals, vdefaults, arg_tuple);

 pyerr:
  psyco_virtualize_exception(po);
  return NULL;
}


 /***************************************************************/
/***   PsycoFunctionObjects                                    ***/
 /***************************************************************/

DEFINEFN
PsycoFunctionObject* psyco_PsycoFunction_NewEx(PyCodeObject* code,
					       PyObject* globals,
					       PyObject* defaults,
					       int rec)
{
	PsycoFunctionObject* result = PyObject_GC_New(PsycoFunctionObject,
						      &PsycoFunction_Type);
	if (result != NULL) {
		result->psy_code = code;         Py_INCREF(code);
		result->psy_globals = globals;   Py_INCREF(globals);
		result->psy_defaults = NULL;
		result->psy_recursion = rec;
		result->psy_fastcall = PyDict_New();
		PyObject_GC_Track(result);

		if (result->psy_fastcall == NULL) {
			Py_DECREF(result);
			return NULL;
		}

		if (defaults != NULL) {
			if (!PyTuple_Check(defaults)) {
				Py_DECREF(result);
				PyErr_SetString(PyExc_PsycoError,
						"Psyco proxies need a tuple "
						"for default arguments");
				return NULL;
			}
			if (PyTuple_GET_SIZE(defaults) > 0) {
				result->psy_defaults = defaults;
				Py_INCREF(defaults);
			}
		}
	}
	return result;
}

DEFINEFN
PsycoFunctionObject* psyco_PsycoFunction_New(PyFunctionObject* func, int rec)
{
	return psyco_PsycoFunction_NewEx((PyCodeObject*) func->func_code,
					 func->func_globals,
					 func->func_defaults,
					 rec);
}

DEFINEFN
PyObject* psyco_proxycode(PyFunctionObject* func, int rec)
{
  PsycoFunctionObject *psyco_fun;
  PyCodeObject *code, *newcode;
  PyObject *consts, *proxy_cobj;
  static PyObject *varnames = NULL;
  static PyObject *free_cell_vars = NULL;
  unsigned char proxy_bytecode[] = {
    LOAD_CONST, 1, 0,
    LOAD_FAST, 0, 0,
    LOAD_FAST, 1, 0,
    CALL_FUNCTION_VAR_KW, 0, 0,
    RETURN_VALUE
  };
  code = (PyCodeObject *)func->func_code;
  if (is_proxycode(code))
    {
      /* already a proxy code object */
      Py_INCREF(code);
      return (PyObject*) code;
    }
  if (PyTuple_Size(code->co_freevars) > 0)
    {
      /* it would be dangerous to continue in this case: the calling
         convention changes when a function has free variables */
      PyErr_SetString(PyExc_PsycoError, "function has free variables");
      return NULL;
    }

  newcode = NULL;
  consts = NULL;
  proxy_cobj = NULL;
  psyco_fun = psyco_PsycoFunction_NewEx(code,
					func->func_globals,
					func->func_defaults,
					rec);
  if (psyco_fun == NULL)
    goto error;

  consts = PyTuple_New(2);
  if (consts == NULL)
    goto error;
  Py_INCREF(Py_None);
  PyTuple_SET_ITEM(consts, 0, Py_None);  /* if a __doc__ is expected there */
  PyTuple_SET_ITEM(consts, 1, (PyObject *)psyco_fun);  /* consumes reference */
  psyco_fun = NULL;

  if (varnames == NULL)
    {
      if (free_cell_vars == NULL)
        {
          free_cell_vars = PyTuple_New(0);
          if (free_cell_vars == NULL)
            goto error;
        }
      varnames = Py_BuildValue("ss", "args", "kwargs");
      if (varnames == NULL)
        goto error;
    }

  proxy_cobj = PyString_FromStringAndSize(proxy_bytecode,
					  sizeof(proxy_bytecode));
  if (proxy_cobj == NULL)
    goto error;

  newcode = PyCode_New(0, 2, 3,
                       CO_OPTIMIZED|CO_NEWLOCALS|CO_VARARGS|CO_VARKEYWORDS,
                       proxy_cobj, consts,
		       varnames, varnames, free_cell_vars,
		       free_cell_vars, code->co_filename,
		       code->co_name, code->co_firstlineno,
		       code->co_lnotab);
  /* fall through */
 error:
  Py_XDECREF(psyco_fun);
  Py_XDECREF(proxy_cobj);
  Py_XDECREF(consts);
  return (PyObject*) newcode;
}

static void psycofunction_dealloc(PsycoFunctionObject* self)
{
	PyObject_GC_UnTrack(self);
	Py_XDECREF(self->psy_fastcall);
	Py_XDECREF(self->psy_defaults);
	Py_DECREF(self->psy_globals);
	Py_DECREF(self->psy_code);
	PyObject_GC_Del(self);
}

#if 0
/* Disabled -- not supposed to be seen at user level */
static PyObject* psycofunction_repr(PsycoFunctionObject* self)
{
	char buf[100];
	if (self->psy_func->func_name == Py_None)
		sprintf(buf, "<anonymous psyco function at %p>", self);
	else
		sprintf(buf, "<psyco function %s at %p>",
			PyString_AsString(self->psy_func->func_name), self);
	return PyString_FromString(buf);
}
#endif

static PyObject* psycofunction_call(PsycoFunctionObject* self,
				    PyObject* arg, PyObject* kw)
{
	PyObject* codebuf;
	PyObject* result;
	PyObject* key;
        long* initial_stack;
	int i;

	if (kw != NULL && PyDict_Check(kw) && PyDict_Size(kw) > 0) {
		/* keyword arguments not supported yet */
		goto unsupported;
	}

	i = PyTuple_GET_SIZE(arg);
	key = PyInt_FromLong(i);
	if (key == NULL)
		return NULL;
	codebuf = PyDict_GetItem(self->psy_fastcall, key);
	
	if (codebuf == NULL) {
		/* not already seen with this number of arguments */
		vinfo_t* vglobals;
		vinfo_array_t* vdefaults;
		vinfo_array_t* arginfo;
		PsycoObject* po;
		source_known_t* sk;

		/* make an array of run-time values */
		arginfo = array_new(i);
		while (i--) {
			/* arbitrary values for the source */
			arginfo->items[i] = vinfo_new(SOURCE_DUMMY);
		}

		/* build the globals and defaults as compile-time values */
		Py_INCREF(self->psy_globals);
		sk = sk_new((long) self->psy_globals, SkFlagPyObj);
		vglobals = vinfo_new(CompileTime_NewSk(sk));
		
		if (self->psy_defaults == NULL)
			vdefaults = NullArray;
		else {
			i = PyTuple_GET_SIZE(self->psy_defaults);
			vdefaults = array_new(i);
			while (i--) {
				PyObject* v = PyTuple_GET_ITEM(
							self->psy_defaults, i);
				Py_INCREF(v);
				sk = sk_new((long) v, SkFlagPyObj);
				vdefaults->items[i] =
					vinfo_new(CompileTime_NewSk(sk));
			}
		}
		
		/* make a "frame" */
		po = psyco_build_frame(self->psy_code, vglobals,
                                       arginfo->items, arginfo->count,
                                       vdefaults->items, vdefaults->count,
				       self->psy_recursion, NULL);
		array_delete(vdefaults, NULL);
		vinfo_decref(vglobals, NULL);
		array_delete(arginfo, NULL);
		
		if (po == BF_UNSUPPORTED) {
			codebuf = Py_None;
			Py_INCREF(codebuf);
		}
		else {
			if (po == NULL) {
				Py_DECREF(key);
				return NULL;
			}
			
			/* compile the function */
			codebuf = (PyObject*) psyco_compile_code(po,
				psyco_first_merge_point(po->pr.merge_points));
		}
		/* cache 'codebuf' (note that this is not necessary, as
		   multiple calls to psyco_compile_code() will just return
		   the same codebuf, but it makes things faster because we
		   don't have to build a whole PsycoObject the next time. */
		if (PyDict_SetItem(self->psy_fastcall, key, codebuf))
			PyErr_Clear(); /* not fatal, ignore error */
	}
	else
		Py_INCREF(codebuf);
	Py_DECREF(key);

	if (codebuf == Py_None) {
		Py_DECREF(codebuf);
		goto unsupported;
	}
	
	/* get the actual arguments */
	initial_stack = (long*) (&PyTuple_GET_ITEM(arg, 0));

	/* run! */
	result = psyco_processor_run((CodeBufferObject*) codebuf, initial_stack,
				     PyTuple_GET_SIZE(arg));
	
	Py_DECREF(codebuf);
	psyco_trash_object(NULL);  /* free any trashed object now */

#if CODE_DUMP >= 2
        psyco_dump_code_buffers();
#endif

	if (result==NULL)
		extra_assert(PyErr_Occurred());
	else
		extra_assert(!PyErr_Occurred());
	return result;

   unsupported:

#if HAVE_PyEval_EvalCodeEx   /* Python >= 2.2b1 */
	{	/* Code copied from function_call() in funcobject.c */
		PyObject **d, **k;
		int nk, nd;

		PyObject* argdefs = self->psy_defaults;
		if (argdefs != NULL) {
			d = &PyTuple_GET_ITEM((PyTupleObject *)argdefs, 0);
			nd = PyTuple_Size(argdefs);
		}
		else {
			d = NULL;
			nd = 0;
		}

		if (kw != NULL && PyDict_Check(kw)) {
			int pos, i;
			nk = PyDict_Size(kw);
			k = PyMem_NEW(PyObject *, 2*nk);
			if (k == NULL) {
				PyErr_NoMemory();
				return NULL;
			}
			pos = i = 0;
			while (PyDict_Next(kw, &pos, &k[i], &k[i+1]))
				i += 2;
			nk = i/2;
		}
		else {
			k = NULL;
			nk = 0;
		}
		
		result = PyEval_EvalCodeEx(self->psy_code,
			self->psy_globals, (PyObject *)NULL,
			&PyTuple_GET_ITEM(arg, 0), PyTuple_Size(arg),
			k, nk, d, nd,
			NULL);

		if (k != NULL)
			PyMem_DEL(k);

		return result;
	}
#else /* !HAVE_PyEval_EvalCodeEx */
	{	/* Work-around for the missing PyEval_EvalCodeEx() */
		PyObject* result = NULL;
		PyObject* hack = PyFunction_New((PyObject*) self->psy_code,
						self->psy_globals);
		if (hack != NULL) {
			if (self->psy_defaults == NULL ||
			    !PyFunction_SetDefaults(hack, self->psy_defaults))
				result = PyEval_CallObjectWithKeywords(hack,
								       arg, kw);
			Py_DECREF(hack);
		}
		return result;
	}
#endif /* HAVE_PyEval_EvalCodeEx */
}

#ifdef Py_TPFLAGS_HAVE_GC
static int psycofunction_traverse(PsycoFunctionObject *f,
				  visitproc visit, void *arg)
{
	int err;
	if (f->psy_code) {
		err = visit((PyObject*) f->psy_code, arg);
		if (err)
			return err;
	}
	if (f->psy_globals) {
		err = visit(f->psy_globals, arg);
		if (err)
			return err;
	}
	if (f->psy_defaults) {
		err = visit(f->psy_defaults, arg);
		if (err)
			return err;
	}
	return 0;
}
#endif /* Py_TPFLAGS_HAVE_GC */

#if 0
/* Disabled. This used to wrap a Psyco proxy around a method object when
   loaded from a class. Now Psyco proxies are only supposed to appear in
   special code objects. */
static PyObject *
psy_descr_get(PyObject *self, PyObject *obj, PyObject *type)
{
	/* 'self' is actually a PsycoFunctionObject* */
	if (obj == Py_None)
		obj = NULL;
	return PyMethod_New(self, obj, type);
}
#endif

DEFINEVAR
PyTypeObject PsycoFunction_Type = {
	PyObject_HEAD_INIT(NULL)
	0,					/*ob_size*/
	"Psyco_function",			/*tp_name*/
	sizeof(PsycoFunctionObject),		/*tp_basicsize*/
	0,					/*tp_itemsize*/
	/* methods */
	(destructor)psycofunction_dealloc,	/*tp_dealloc*/
	0,					/*tp_print*/
	0,					/*tp_getattr*/
	0,					/*tp_setattr*/
	0,					/*tp_compare*/
	0,					/*tp_repr*/
	0,					/*tp_as_number*/
	0,					/*tp_as_sequence*/
	0,					/*tp_as_mapping*/
	0,					/*tp_hash*/
	(ternaryfunc)psycofunction_call,	/*tp_call*/
	0,					/* tp_str */
	0,					/* tp_getattro */
	0,					/* tp_setattro */
#ifdef Py_TPFLAGS_HAVE_GC
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
	0,					/* tp_doc */
	(traverseproc)psycofunction_traverse,	/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	0,					/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
#endif /* Py_TPFLAGS_HAVE_GC */
};


 /***************************************************************/
/***   Implementation of the '_psyco' built-in module          ***/
 /***************************************************************/

#if CODE_DUMP
static void vinfo_array_dump(vinfo_array_t* array, FILE* f, PyObject* d)
{
  int i = array->count;
  fprintf(f, "%d\n", i);
  while (i--)
    {
      vinfo_t* vi = array->items[i];
      PyObject* key = PyInt_FromLong((long)vi);
      assert(key);
      fprintf(f, "%ld\n", (long)vi);
      if (vi != NULL && !PyDict_GetItem(d, key))
        {
          switch (gettime(vi->source)) {
          case CompileTime:
            fprintf(f, "ct %ld %ld\n",
                    CompileTime_Get(vi->source)->refcount1_flags,
                    CompileTime_Get(vi->source)->value);
            break;
          case RunTime:
            fprintf(f, "rt %ld\n", vi->source);
            break;
          case VirtualTime:
            fprintf(f, "vt 0x%lx\n", (long) VirtualTime_Get(vi->source));
            break;
          default:
            assert(0);
          }
          PyDict_SetItem(d, key, Py_None);
          vinfo_array_dump(vi->array, f, d);
        }
      Py_DECREF(key);
    }
}
DEFINEFN
void psyco_dump_code_buffers(void)
{
  FILE* f = fopen(CODE_DUMP_FILE, "wb");
  if (f != NULL)
    {
      CodeBufferObject* obj;
      PyObject *exc, *val, *tb;
      void** chain;
      int bufcount = 0;
      long* buftable;
#ifdef CODE_DUMP_SYMBOLS
      int i1;
      PyObject* global_addrs = PyDict_New();
      assert(global_addrs);
#endif
      PyErr_Fetch(&exc, &val, &tb);
      debug_printf(("psyco: writing " CODE_DUMP_FILE "\n"));

      for (obj=psyco_codebuf_chained_list; obj != NULL; obj=obj->chained_list)
        bufcount++;
      buftable = (long*) PyCore_MALLOC(bufcount*sizeof(long));
      fwrite(&bufcount, sizeof(bufcount), 1, f);
      fwrite(buftable, sizeof(long), bufcount, f);

      /* give the address of an arbitrary symbol from the Python interpreter
         and from the Psyco module */
      fprintf(f, "PyInt_FromLong: 0x%lx\n", (long) &PyInt_FromLong);
      fprintf(f, "psyco_dump_code_buffers: 0x%lx\n",
              (long) &psyco_dump_code_buffers);

      for (obj=psyco_codebuf_chained_list; obj != NULL; obj=obj->chained_list)
        {
          int nsize = obj->codeend - obj->codeptr;
          PyCodeObject* co = obj->snapshot.fz_pyc_data ?
		  obj->snapshot.fz_pyc_data->co : NULL;
          fprintf(f, "CodeBufferObject 0x%lx %d %d '%s' '%s' %d '%s'\n",
                  (long) obj->codeptr, nsize, get_stack_depth(&obj->snapshot),
                  co?PyString_AsString(co->co_filename):"",
                  co?PyCodeObject_NAME(co):"",
                  co?obj->snapshot.fz_pyc_data->next_instr:-1,
                  obj->codemode);
          fwrite(obj->codeptr, 1, nsize, f);
#ifdef CODE_DUMP_SYMBOLS
          /* look-up all potential 'void*' pointers appearing in the code */
          for (i1=0; i1+sizeof(void*)<=nsize; i1++)
            {
              PyObject* key = PyInt_FromLong(*(long*)(obj->codeptr+i1));
              assert(key);
              PyDict_SetItem(global_addrs, key, Py_None);
              Py_DECREF(key);
            }
#endif
        }

      for (chain = psyco_codebuf_spec_dict_list; chain; chain=(void**)*chain)
        {
          PyObject* spec_dict = (PyObject*)(chain[-1]);
          int i = 0;
          PyObject *key, *value;
          fprintf(f, "spec_dict 0x%lx\n", (long) chain);
          while (PyDict_Next(spec_dict, &i, &key, &value))
            {
              PyObject* repr;
              if (PyInt_Check(key))
                {
#ifdef CODE_DUMP_SYMBOLS
                  PyDict_SetItem(global_addrs, key, Py_None);
#endif
                  repr = (key->ob_type->tp_as_number->nb_hex)(key);
                }
              else
                {
#ifdef CODE_DUMP_SYMBOLS
                  key = PyInt_FromLong((long) key);
                  assert(key);
                  PyDict_SetItem(global_addrs, key, Py_None);
#endif
                  repr = PyObject_Repr(key);
#ifdef CODE_DUMP_SYMBOLS
                  Py_DECREF(key);
#endif
                }
              assert(!PyErr_Occurred());
              assert(PyString_Check(repr));
              assert(CodeBuffer_Check(value));
              fprintf(f, "0x%lx %s\n", (long)((CodeBufferObject*)value)->codeptr,
                      PyString_AsString(repr));
              Py_DECREF(repr);
            }
          fprintf(f, "\n");
        }
#ifdef CODE_DUMP_SYMBOLS
      {
        int i = 0;
        PyObject *key, *value;
        fprintf(f, "symbol table\n");
        while (PyDict_Next(global_addrs, &i, &key, &value))
          {
            Dl_info info;
            void* ptr = (void*) PyInt_AS_LONG(key);
            if (dladdr(ptr, &info) && ptr == info.dli_saddr)
              fprintf(f, "0x%lx %s\n", (long) ptr, info.dli_sname);
          }
        Py_DECREF(global_addrs);
      }
#endif
      {
        int i;
        fprintf(f, "vinfo_array\n");
        i = 0;
        for (obj=psyco_codebuf_chained_list; obj != NULL; obj=obj->chained_list)
          {
            PyObject* d = PyDict_New();
            assert(d);
            buftable[i++] = ftell(f);
            vinfo_array_dump(obj->snapshot.fz_vlocals, f, d);
            Py_DECREF(d);
          }
        assert(i==bufcount);
        fseek(f, sizeof(bufcount), 0);
        fwrite(buftable, sizeof(long), bufcount, f);
      }
      PyCore_FREE(buftable);
      assert(!PyErr_Occurred());
      fclose(f);
      PyErr_Restore(exc, val, tb);
    }
}
static PyObject* Psyco_dumpcodebuf(PyObject* self, PyObject* args)
{
  psyco_dump_code_buffers();
  Py_INCREF(Py_None);
  return Py_None;
}
#endif  /* CODE_DUMP */

#if VERBOSE_LEVEL >= 4
DEFINEFN void psyco_trace_execution(char* msg, void* code_position)
{
  debug_printf(("psyco: trace %p for %s\n", code_position, msg));
}
#endif


/*****************************************************************/

static PyObject* Psyco_proxycode(PyObject* self, PyObject* args)
{
	int recursion = DEFAULT_RECURSION;
	PyFunctionObject* function;
	
	if (!PyArg_ParseTuple(args, "O!|i",
			      &PyFunction_Type, &function,
			      &recursion))
		return NULL;

	return psyco_proxycode(function, recursion);
}

/* Initialize selective compilation */
static PyObject* Psyco_selective(PyObject* self, PyObject* args)
{
  if (!PyArg_ParseTuple(args, "i", &ticks)) {
    return NULL;
  }

  /* Sanity check argument */
  if (ticks < 0) {
    PyErr_SetString(PyExc_ValueError, "negative ticks");
    return NULL;
  }

  /* Enable our selective compilation function */
  if (psyco_start_selective())
    return NULL;
  
  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************/

static char proxycode_doc[] =
"proxycode(func[, rec]) -> code object\n\
\n\
Return a proxy code object that invokes Psyco on the argument. The code\n\
object can be used to replace func.func_code. Raise psyco.error if func\n\
uses unsupported features. Return func.func_code itself if it is already\n\
a proxy code object. The optional second argument specifies the number of\n\
recursive compilation levels: all functions called by func are compiled\n\
up to the given depth of indirection.";

static PyMethodDef PsycoMethods[] = {
	{"proxycode",	&Psyco_proxycode,	METH_VARARGS,	proxycode_doc},
	{"selective",   &Psyco_selective,	METH_VARARGS},
#if CODE_DUMP
	{"dumpcodebuf",	&Psyco_dumpcodebuf,	METH_VARARGS},
#endif
	{NULL,		NULL}        /* Sentinel */
};

/* Initialization */
void init_psyco(void)
{
  PyObject* CPsycoModule;

  PsycoFunction_Type.ob_type = &PyType_Type;
  CodeBuffer_Type.ob_type = &PyType_Type;

  CPsycoModule = Py_InitModule("_psyco", PsycoMethods);
  if (CPsycoModule == NULL)
    return;
  PyExc_PsycoError = PyErr_NewException("psyco.error", NULL, NULL);
  if (PyExc_PsycoError == NULL)
    return;
  if (PyModule_AddObject(CPsycoModule, "error", PyExc_PsycoError))
    return;
  Py_INCREF(&PsycoFunction_Type);
  if (PyModule_AddObject(CPsycoModule, "PsycoFunctionType",
			 (PyObject*) &PsycoFunction_Type))
    return;
  if (PyModule_AddIntConstant(CPsycoModule, "DEFAULT_RECURSION", DEFAULT_RECURSION))
    return;
#if ALL_CHECKS
  if (PyModule_AddIntConstant(CPsycoModule, "ALL_CHECKS", ALL_CHECKS))
    return;
#endif
#if VERBOSE_LEVEL
  if (PyModule_AddIntConstant(CPsycoModule, "VERBOSE_LEVEL", VERBOSE_LEVEL))
    return;
#endif
#ifdef PY_PSYCO_MODULE
  PyPsycoModule = PyImport_ImportModule("psyco");
  if (PyPsycoModule == NULL)
    return;
#endif

  initialize_all_files();
}
