#include "../processor.h"
#include "../vcompiler.h"
#include "../dispatcher.h"
#include "../codemanager.h"
#include "../Python/pycompiler.h"  /* for exception handling stuff */
#include "../Python/frames.h"
#include "../timing.h"
#include "ipyencoding.h"


/* define to copy static machine code in the heap before running it.
   I've seen some Linux distributions in which the static data pages
   are not executable by default. */
#define COPY_CODE_IN_HEAP


/* We make no special use of any register but ESP, and maybe EBP
 * (if EBP_IF_RESERVED).
 * We consider that we can call C functions with arbitrary values in
 * all registers but ESP, and that only EAX, ECX and EDX will be
 * clobbered.
 * We do not use EBP as the frame pointer, unlike normal C compiled
 * functions. This makes instruction encodings one byte longer
 * (ESP-relative instead of EBP-relative).
 */

DEFINEVAR
reg_t RegistersLoop[REG_TOTAL] = {
  /* following EAX: */  REG_386_ECX,
  /* following ECX: */  REG_386_EDX,
  /* following EDX: */  REG_386_EBX,
  /* following EBX: */  EBP_IS_RESERVED ? REG_386_ESI : REG_386_EBP,
  /* following ESP: */  REG_NONE,
  /* following EBP: */  EBP_IS_RESERVED ? REG_NONE : REG_386_ESI,
  /* following ESI: */  REG_386_EDI,
  /* following EDI: */  REG_386_EAX };


/* glue code for psyco_processor_run(). */
static code_t glue_run_code[] = {
  0x8B, 0x44, 0x24, 4,          /*   MOV EAX, [ESP+4]  (code target)   */
  0x8B, 0x4C, 0x24, 8,          /*   MOV ECX, [ESP+8]  (stack end)     */
  0x8B, 0x54, 0x24, 12,         /*   MOV EDX, [ESP+12] (initial stack) */
  PUSH_REG_INSTR(REG_386_EBP),  /*   PUSH EBP        */
  PUSH_REG_INSTR(REG_386_EBX),  /*   PUSH EBX        */
  PUSH_REG_INSTR(REG_386_ESI),  /*   PUSH ESI        */
  PUSH_REG_INSTR(REG_386_EDI),  /*   PUSH EDI        */
  0x8B, 0x5C, 0x24, 32,         /*   MOV EBX, [ESP+32] (finfo frame stack ptr) */
  0x6A, -1,                     /*   PUSH -1         */
  0x89, 0x23,                   /*   MOV [EBX], ESP  */
  0xEB, +5,                     /*   JMP Label2      */
                                /* Label1:           */
  0x83, 0xE9, 4,                /*   SUB ECX, 4      */
  0xFF, 0x31,                   /*   PUSH [ECX]      */
                                /* Label2:           */
  0x39, 0xCA,                   /*   CMP EDX, ECX    */
  0x75, -9,                     /*   JNE Label1      */
  0xFF, 0xD0,                   /*   CALL *EAX     (callee removes args)  */
  POP_REG_INSTR(REG_386_EDI),   /*   POP EDI         */
  POP_REG_INSTR(REG_386_ESI),   /*   POP ESI         */
  POP_REG_INSTR(REG_386_EBX),   /*   POP EBX         */
  POP_REG_INSTR(REG_386_EBP),   /*   POP EBP         */
  0xC3,                         /*   RET             */
};

typedef PyObject* (*glue_run_code_fn) (code_t* code_target,
				       long* stack_end,
				       long* initial_stack,
				       struct stack_frame_info_s*** finfo);

#ifdef COPY_CODE_IN_HEAP
static glue_run_code_fn glue_run_code_1;
#else
# define glue_run_code_1 ((glue_run_code_fn) glue_run_code)
#endif

DEFINEFN
PyObject* psyco_processor_run(CodeBufferObject* codebuf,
                              long initial_stack[],
                              struct stack_frame_info_s*** finfo)
{
  int argc = RUN_ARGC(codebuf);
  return glue_run_code_1(codebuf->codestart, initial_stack + argc,
                         initial_stack, finfo);
}

/* call a C function with a variable number of arguments */
DEFINEVAR long (*psyco_call_var) (void* c_func, int argcount, long arguments[]);

static code_t glue_call_var[] = {
	0x53,			/*   PUSH EBX                      */
	0x8B, 0x5C, 0x24, 12,	/*   MOV EBX, [ESP+12]  (argcount) */
	0x8B, 0x44, 0x24, 8,	/*   MOV EAX, [ESP+8]   (c_func)   */
	0x09, 0xDB,		/*   OR EBX, EBX                   */
	0x74, +16,		/*   JZ Label1                     */
	0x8B, 0x54, 0x24, 16,	/*   MOV EDX, [ESP+16] (arguments) */
	0x8D, 0x0C, 0x9A,	/*   LEA ECX, [EDX+4*EBX]          */
				/* Label2:                         */
	0x83, 0xE9, 4,		/*   SUB ECX, 4                    */
	0xFF, 0x31,		/*   PUSH [ECX]                    */
	0x39, 0xCA,		/*   CMP EDX, ECX                  */
	0x75, -9,		/*   JNE Label2                    */
				/* Label1:                         */
	0xFF, 0xD0,		/*   CALL *EAX                     */
	0x8D, 0x24, 0x9C,	/*   LEA ESP, [ESP+4*EBX]          */
	0x5B,			/*   POP EBX                       */
	0xC3,			/*   RET                           */
};

/* check for signed integer multiplication overflow */
static code_t glue_int_mul[] = {
  0x8B, 0x44, 0x24, 8,          /*   MOV  EAX, [ESP+8]  (a)   */
  0x0F, 0xAF, 0x44, 0x24, 4,    /*   IMUL EAX, [ESP+4]  (b)   */
  0x0F, 0x90, 0xC0,             /*   SETO AL                  */
  0xC3,                         /*   RET                      */
};

typedef char (*glue_int_mul_fn) (long a, long b);

#ifdef COPY_CODE_IN_HEAP
static glue_int_mul_fn glue_int_mul_1;
#else
# define glue_int_mul_1 ((glue_int_mul_fn) glue_int_mul)
#endif


#ifdef PENTIUM_TSC  /* if itiming.h is included by timing.h */
static code_t glue_pentium_tsc[] = {
  0x0F, 0x31,                   /*   RDTSC   */
  0xC3,                         /*   RET     */
};
DEFINEVAR psyco_pentium_tsc_fn psyco_pentium_tsc;
#endif /* PENTIUM_TSC */


DEFINEFN
void psyco_emit_header(PsycoObject* po, int nframelocal)
{
  int j = nframelocal;
  vinfo_array_t* array;
  extra_assert(LOC_CONTINUATION->array->count == 0);

  BEGIN_CODE
  INITIALIZE_FRAME_LOCALS(nframelocal);
  po->stack_depth += 4*nframelocal;
  END_CODE

  array = LOC_CONTINUATION->array = array_new(nframelocal);
  while (j--)
    array->items[j] = vinfo_new(RunTime_NewStack
                             (po->stack_depth - 4*j, REG_NONE, false, false));
}

DEFINEFN
code_t* psyco_finish_return(PsycoObject* po, Source retval)
{
  code_t* code = po->code;
  int retpos;
  int nframelocal = LOC_CONTINUATION->array->count;

  /* 'retpos' is the position in the stack of the return address. */
  retpos = getstack(LOC_CONTINUATION->source);
  extra_assert(retpos != RunTime_StackNone);

  /* load the return value into EAX for regular functions, EBX for functions
     with a prologue */
  if (retval != SOURCE_DUMMY) {
    reg_t rg = nframelocal>0 ? REG_ANY_CALLEE_SAVED : REG_FUNCTIONS_RETURN;
    LOAD_REG_FROM(retval, rg);
  }

  if (nframelocal > 0)
    {
      /* psyco_emit_header() was used; first clear the stack only up to and not
         including the frame-local data */
      int framelocpos = retpos + 4*nframelocal;
      extra_assert(framelocpos ==
                   getstack(LOC_CONTINUATION->array->items[0]->source));
      STACK_CORRECTION(framelocpos - po->stack_depth);
      po->stack_depth = framelocpos;
      
      /* perform Python-specific cleanup */
      FINALIZE_FRAME_LOCALS(nframelocal);
      LOAD_REG_FROM_REG(REG_FUNCTIONS_RETURN, REG_ANY_CALLEE_SAVED);
    }

  /* now clean up the stack up to retpos */
  STACK_CORRECTION(retpos - po->stack_depth);

  /* emit a 'RET xxx' instruction that pops and jumps to the address
     which is now at the top of the stack, and finishes to clean the
     stack by removing everything left past the return address
     (typically the arguments, although it could be anything). */
  retpos -= INITIAL_STACK_DEPTH;
  extra_assert(0<retpos);
  if (retpos >= 0x8000)
    {
      /* uncommon case: too many stuff left in the stack for the 16-bit
         immediate we can encoding in RET */
      POP_REG(REG_386_EDX);
      STACK_CORRECTION(-retpos);
      PUSH_REG(REG_386_EDX);
      retpos = 0;
    }
  code[0] = 0xC2;   // RET imm16
  *(short*)(code+1) = retpos;
  PsycoObject_Delete(po);
  return code+3;
}

#if 0   /* disabled */
DEFINEFN
code_t* psyco_emergency_jump(PsycoObject* po, code_t* code)
{
  STACK_CORRECTION(INITIAL_STACK_DEPTH - po->stack_depth);  /* at most 6 bytes */
  code[0] = 0xE9;   /* JMP rel32 */
  code += 5;
  *(long*)(code-4) = ((code_t*)(&PyErr_NoMemory)) - code;
  /* total: at most 11 bytes. Check the value of EMERGENCY_PROXY_SIZE. */
  return code;
}
#endif

DEFINEFN
void* psyco_call_code_builder(PsycoObject* po, void* fn, int restore,
                              RunTimeSource extraarg)
{
  code_t* code = po->code;
  void* result;
  code_t* fixvalue;

  if (restore)
    TEMP_SAVE_REGS_FN_CALLS;
  else
    SAVE_REGS_FN_CALLS;

  /* first pushed argument */
  if (extraarg != SOURCE_DUMMY)
    CALL_SET_ARG_FROM_RT(extraarg, 1, 2);  /* argument index 1 out of total 2 */
  
  /* last pushed argument (will be the first argument of 'fn') */
  code[0] = 0x68;     /* PUSH IMM32	*/
  fixvalue = code+1;    /* will be filled below */
  code[5] = 0xE8;     /* CALL fn	*/
  code += 10;
  *(long*)(code-4) = ((code_t*)fn) - code;

  if (restore)
    {
      /* cancel the effect of CALL_SET_ARG_FROM_RT on po->stack_depth,
         to match the 'ADD ESP' instruction below */
      int nb_args = 1 + (extraarg != SOURCE_DUMMY);
      po->stack_depth -= 4*(nb_args-1);
      
      extra_assert(4*nb_args < 128);   /* trivially true */
      CODE_FOUR_BYTES(code,
                      0x83,       /* ADD		  */
                      0xC4,       /*     ESP,		  */
                      4*nb_args,  /*           4*nb_args  */
                      0);         /* not used             */
      code += 3;
      TEMP_RESTORE_REGS_FN_CALLS_AND_JUMP;
    }
  else
    {
      po->stack_depth += 4;  /* for the PUSH IMM32 above */
      code[0] = 0xFF;      /* JMP *EAX */
      code[1] = 0xE0;
      code += 2;
    }

    /* make 'fs' point just after the end of the code, aligned */
  result = (void*)(((long)code + 3) & ~ 3);
#if CODE_DUMP
  while (code != (code_t*) result)
    *code++ = (code_t) 0xCC;   /* fill with INT 3 (debugger trap) instructions */
#endif
  *(void**)fixvalue = result;    /* set value at code+1 above */
  return result;
}


/* run-time vinfo_t creation */
inline vinfo_t* new_rtvinfo(PsycoObject* po, reg_t reg, bool ref, bool nonneg) {
	vinfo_t* vi = vinfo_new(RunTime_New(reg, ref, nonneg));
	REG_NUMBER(po, reg) = vi;
	return vi;
}

inline code_t* write_modrm(code_t* code, code_t middle,
                           reg_t base, reg_t index, int shift,
                           long offset)
{
  /* write a mod/rm encoding. */
  extra_assert(index != REG_386_ESP);
  extra_assert(0 <= shift && shift < 4);
  if (base == REG_NONE)
    {
      if (index == REG_NONE)
        {
          code[0] = middle | 0x05;
          *(long*)(code+1) = offset;
          return code+5;
        }
      else
        {
          code[0] = middle | 0x04;
          code[1] = (shift<<6) | (index<<3) | 0x05;
          *(long*)(code+2) = offset;
          return code+6;
        }
    }
  else if (index == REG_NONE)
    {
      if (base == REG_386_ESP)
        {
          code[0] = 0x84 | middle;
          code[1] = 0x24;
          *(long*)(code+2) = offset;
          return code+6;
        }
      else if (COMPACT_ENCODING && offset == 0 && base!=REG_386_EBP)
        {
          code[0] = middle | base;
          return code+1;
        }
      else if (COMPACT_ENCODING && offset < 128)
        {
          extra_assert(offset >= 0);
          code[0] = 0x40 | middle | base;
          code[1] = (code_t) offset;
          return code+2;
        }
      else
        {
          code[0] = 0x80 | middle | base;
          *(long*)(code+1) = offset;
          return code+5;
        }
    }
  else
    {
      code[1] = (shift<<6) | (index<<3) | base;
      if (COMPACT_ENCODING && offset == 0 && base!=REG_386_EBP)
        {
          code[0] = middle | 0x04;
          return code+2;
        }
      else if (COMPACT_ENCODING && offset < 128)
        {
          extra_assert(offset >= 0);
          code[0] = middle | 0x44;
          code[2] = (code_t) offset;
          return code+3;
        }
      else
        {
          code[0] = middle | 0x84;
          *(long*)(code+2) = offset;
          return code+6;
        }
    }
}

#define NewOutputRegister  ((vinfo_t*) 1)

static reg_t mem_access(PsycoObject* po, code_t opcodes[], vinfo_t* nv_ptr,
                        long offset, vinfo_t* rt_vindex, int size2,
                        vinfo_t* rt_extra)
{
  int i;
  reg_t basereg, indexreg, extrareg;

  BEGIN_CODE
  if (is_runtime(nv_ptr->source))
    {
      RTVINFO_IN_REG(nv_ptr);
      basereg = RUNTIME_REG(nv_ptr);
    }
  else
    {
      offset += CompileTime_Get(nv_ptr->source)->value;
      basereg = REG_NONE;
    }
  
  if (rt_vindex != NULL)
    {
      DELAY_USE_OF(basereg);
      RTVINFO_IN_REG(rt_vindex);
      indexreg = RUNTIME_REG(rt_vindex);
    }
  else
    indexreg = REG_NONE;

  if (rt_extra == NULL)
    extrareg = 0;
  else
    {
      DELAY_USE_OF_2(basereg, indexreg);
      if (rt_extra == NewOutputRegister)
        NEED_FREE_REG(extrareg);
      else
        {
          if (size2==0)
            RTVINFO_IN_BYTE_REG(rt_extra, basereg, indexreg);
          else
            RTVINFO_IN_REG(rt_extra);
          extrareg = RUNTIME_REG(rt_extra);
        }
    }
  
  for (i = *opcodes++; i--; ) *code++ = *opcodes++;
  code = write_modrm(code, (code_t)(extrareg<<3), basereg, indexreg, size2, offset);
  for (i = *opcodes++; i--; ) *code++ = *opcodes++;
  END_CODE
  return extrareg;
}

DEFINEFN
vinfo_t* psyco_memory_read(PsycoObject* po, vinfo_t* nv_ptr, long offset,
                           vinfo_t* rt_vindex, int size2, bool nonsigned)
{
  code_t opcodes[4];
  reg_t targetreg;
  switch (size2) {
  case 0:
    /* reading only one byte */
    opcodes[0] = 2;
    opcodes[1] = 0x0F;
    opcodes[2] = nonsigned
      ? 0xB6       /* MOVZX reg, byte [...] */
      : 0xBE;      /* MOVSX reg, byte [...] */
    opcodes[3] = 0;
    break;
  case 1:
    /* reading only two bytes */
    opcodes[0] = 2;
    opcodes[1] = 0x0F;
    opcodes[2] = nonsigned
      ? 0xB7       /* MOVZX reg, short [...] */
      : 0xBF;      /* MOVSX reg, short [...] */
    opcodes[3] = 0;
    break;
  default:
    /* reading a long */
    opcodes[0] = 1;
    opcodes[1] = 0x8B;  /* MOV reg, long [...] */
    opcodes[2] = 0;
    break;
  }
  targetreg = mem_access(po, opcodes, nv_ptr, offset, rt_vindex,
                         size2, NewOutputRegister);
  return new_rtvinfo(po, targetreg, false, false);
}

DEFINEFN
bool psyco_memory_write(PsycoObject* po, vinfo_t* nv_ptr, long offset,
                        vinfo_t* rt_vindex, int size2, vinfo_t* value)
{
  code_t opcodes[8];
  if (!compute_vinfo(value, po)) return false;

  if (is_runtime(value->source))
    {
      switch (size2) {
      case 0:
        /* writing only one byte */
        opcodes[0] = 1;
        opcodes[1] = 0x88;   /* MOV byte [...], reg */
        /* 'reg' is forced in mem_access to be an 8-bit register */
        opcodes[2] = 0;
        break;
      case 1:
        /* writing only two bytes */
        opcodes[0] = 2;
        opcodes[1] = 0x66;
        opcodes[2] = 0x89;   /* MOV short [...], reg */
        opcodes[3] = 0;
        break;
      default:
        /* writing a long */
        opcodes[0] = 1;
        opcodes[1] = 0x89;   /* MOV long [...], reg */
        opcodes[2] = 0;
        break;
      }
    }
  else
    {
      code_t* code1;
      long immed = CompileTime_Get(value->source)->value;
      value = NULL;  /* not run-time */
      switch (size2) {
      case 0:
        /* writing an immediate byte */
        opcodes[0] = 1;
        opcodes[1] = 0xC6;
        opcodes[2] = 1;
        opcodes[3] = (code_t) immed;
        break;
      case 1:
        /* writing an immediate short */
        opcodes[0] = 2;
        opcodes[1] = 0x66;
        opcodes[2] = 0xC7;
        opcodes[3] = 2;
        opcodes[4] = (code_t) immed;
        opcodes[5] = (code_t) (immed >> 8);
        break;
      default:
        /* writing an immediate long */
        code1 = opcodes;  /* workaround for a GCC overoptimization */
        code1[0] = 1;
        code1[1] = 0xC7;
        code1[2] = 4;
        *(long*)(code1+3) = immed;
        break;
      }
    }
  mem_access(po, opcodes, nv_ptr, offset, rt_vindex, size2, value);
  return true;
}


/***************************************************************/
 /*** Condition Codes (a.k.a. the processor 'flags' register) ***/

static source_virtual_t cc_functions_table[CC_TOTAL];

inline condition_code_t cc_from_vsource(Source source)
{
	source_virtual_t* sv = VirtualTime_Get(source);
	return (condition_code_t) (sv - cc_functions_table);
}

/* internal, see NEED_CC() */
DEFINEFN
code_t* psyco_compute_cc(PsycoObject* po, code_t* code, reg_t reserved)
{
	vinfo_t* v = po->ccreg;
	condition_code_t cc = cc_from_vsource(v->source);
	reg_t rg;

	NEED_FREE_BYTE_REG(rg, reserved, REG_NONE);
	LOAD_REG_FROM_CONDITION(rg, cc);

	v->source = RunTime_New(rg, false, true);
	REG_NUMBER(po, rg) = v;
	po->ccreg = NULL;
        return code;
}

static bool generic_computed_cc(PsycoObject* po, vinfo_t* v)
{
	/* also upon forking, because the condition codes cannot be
	   sent as arguments (function calls typically require adds
	   and subs to adjust the stack). */
	extra_assert(po->ccreg == v);
	BEGIN_CODE
	code = psyco_compute_cc(po, code, REG_NONE);
	END_CODE
	return true;
}


DEFINEFN
vinfo_t* psyco_vinfo_condition(PsycoObject* po, condition_code_t cc)
{
  vinfo_t* result;
  if (cc < CC_TOTAL)
    {
      if (po->ccreg != NULL)
        {
          /* there is already a value in the processor flags register */
          extra_assert(psyco_vsource_cc(po->ccreg->source) != CC_ALWAYS_FALSE);
          
          if (psyco_vsource_cc(po->ccreg->source) == cc)
            {
              /* it is the same condition, so reuse it */
              result = po->ccreg;
              vinfo_incref(result);
              return result;
            }
          /* it is not the same condition, save it */
          BEGIN_CODE
          NEED_CC();
          END_CODE
        }
      extra_assert(po->ccreg == NULL);
      po->ccreg = vinfo_new(VirtualTime_New(cc_functions_table+(int)cc));
      result = po->ccreg;
    }
  else
    result = vinfo_new(CompileTime_New(cc == CC_ALWAYS_TRUE));
  return result;
}

DEFINEFN
condition_code_t psyco_vsource_cc(Source source)
{
  if (is_virtualtime(source))
    {
      source_virtual_t* sv = VirtualTime_Get(source);
      if (cc_functions_table <= sv  &&  sv < cc_functions_table+CC_TOTAL)
        {
          /* 'sv' points within the cc_functions_table */
          return cc_from_vsource(source);
        }
    }
  return CC_ALWAYS_FALSE;
}


#ifdef COPY_CODE_IN_HEAP
#  define COPY_CODE(target, source, type)   do {	\
	char* c = PyMem_MALLOC(sizeof(source));		\
	if (c == NULL) {				\
		PyErr_NoMemory();			\
		return;					\
	}						\
	memcpy(c, source, sizeof(source));		\
	target = (type) c;				\
} while (0)
#else
#  define COPY_CODE(target, source, type)   (target = (type) source)
#endif


static bool computed_promotion(PsycoObject* po, vinfo_t* v);
/* forward */

INITIALIZATIONFN
void psyco_processor_init(void)
{
  int i;
#ifdef COPY_CODE_IN_HEAP
  COPY_CODE(glue_run_code_1,    glue_run_code,     glue_run_code_fn);
  COPY_CODE(glue_int_mul_1,     glue_int_mul,      glue_int_mul_fn);
#endif
#ifdef PENTIUM_TSC
  COPY_CODE(psyco_pentium_tsc,  glue_pentium_tsc,  psyco_pentium_tsc_fn);
#endif
  COPY_CODE(psyco_call_var, glue_call_var, long(*)(void*, int, long[]));
  for (i=0; i<CC_TOTAL; i++)
    {
      /* the condition codes cannot be passed across function calls,
         hence the NW_FORCE argument */
      INIT_SVIRTUAL(cc_functions_table[i], generic_computed_cc, 0, NW_FORCE);
    }
  psyco_nonfixed_promotion.header.compute_fn = &computed_promotion;
#if USE_RUNTIME_SWITCHES
  psyco_nonfixed_promotion.fs = NULL;
#endif
  psyco_nonfixed_promotion.kflags = SkFlagFixed;
  psyco_nonfixed_pyobj_promotion.header.compute_fn = &computed_promotion;
#if USE_RUNTIME_SWITCHES
  psyco_nonfixed_pyobj_promotion.fs = NULL;
#endif
  psyco_nonfixed_pyobj_promotion.kflags = SkFlagFixed | SkFlagPyObj;
}


/*****************************************************************/
 /***   run-time switches                                       ***/

static bool computed_promotion(PsycoObject* po, vinfo_t* v)
{
  /* uncomputable, but still use the address of computed_promotion() as a
     tag to figure out if a virtual source is a c_promotion_s structure. */
  return psyco_vsource_not_important.compute_fn(po, v);
}

DEFINEVAR struct c_promotion_s psyco_nonfixed_promotion;
DEFINEVAR struct c_promotion_s psyco_nonfixed_pyobj_promotion;

DEFINEFN
bool psyco_vsource_is_promotion(VirtualTimeSource source)
{
  return VirtualTime_Get(source)->compute_fn == &computed_promotion;
}


#if USE_RUNTIME_SWITCHES

/* The tactic is to precompute, given a list of the values we will
   have to switch on, a binary tree search algorithm in machine
   code. Example for the list of values [10, 20, 30, 40]:

	CMP EAX, 30
	JG L1		; if EAX > 30, jump to L1
	JE Case30	; if EAX == 30, jump to Case30
	CMP EAX, 20
	JG L2		; if 20 < EAX < 30, jump to L2
	JE Case20	; if EAX == 20, jump to Case20
	CMP EAX, 10
	JE Case10	; if EAX == 10, jump to Case10
	JMP L2		; otherwise, jump to L2
   L1:  CMP EAX, 40
	JE Case40	; if EAX == 40, jump to Case40
   L2:  JMP Default     ; <--- 'supposed_end' below
                        ; <--- 'supposed_end+5'

   All targets (Default and Case10 ... Case40) are initially set to
   point to the end of this code block. At compile-time we will put
   at this place a proxy that calls the dispatcher. The dispatcher then
   fixes the target in the code itself as a shortcut for the next time
   that the same value is found.

   The second byte of the CMP instructions must be dynamically corrected
   to mention the actual register, which is not necessarily EAX. This is
   done by coding a linked list in these bytes, each one holding the
   offset to the next CMP's second byte.
*/

struct fxcase_s {
  long value;             /* value to switch on */
  long index;             /* original (unsorted) index of this value */
};

static int fx_compare(const void* a, const void* b)
{
  long va = ((const struct fxcase_s*)a)->value;
  long vb = ((const struct fxcase_s*)b)->value;
  if (va < vb)
    return -1;
  if (va > vb)
    return 1;
  return 0;
}

#define FX_BASE_SIZE        5
#define FX_MAX_ITEM_SIZE    18
#define FX_LAST_JUMP_SIZE   2

static code_t* fx_writecases(code_t* code, code_t** lastcmp,
                             struct fxcase_s* fxc, long* fixtargets,
                             int first, int last, code_t* supposed_end)
{
  /* write the part of the switch corresponding to the cases between
     'first' (inclusive) and 'last' (exclusive).
     '*lastcmp' points to the last CMP instruction's second byte. */
  if (first == last)
    {
      /* jump to 'default:' */
      long offset;
      code += 2;
      offset = supposed_end - code;
      if (offset < 128)
        {
          code[-2] = 0xEB;   /* JMP rel8 */
          code[-1] = (code_t) offset;
        }
      else
        {
          code += 3;
          code[-5] = 0xE9;   /* JMP rel32 */
          *(long*)(code-4) = offset;
        }
    }
  else
    {
      code_t* code2 = code+1;
      long offset;
      int middle = (first+last-1)/2;
      COMMON_INSTR_IMMED(7, 0, fxc[middle].value); /* CMP reg, imm */
      if (*lastcmp != NULL)
        {
          extra_assert(code2 - *lastcmp < 128);   /* CMP instructions are close
                                                    to each other */
          **lastcmp = code2 - *lastcmp;
        }
      *lastcmp = code2;
      if (middle > first)
        {
          code += 2;
          code2 = fx_writecases(code+6, lastcmp, fxc, fixtargets,
                                first, middle, supposed_end);
          offset = code2 - code;
          if (offset < 128)
            {
              code[-2] = 0x7F;    /* JG rel8 */
              code[-1] = (code_t) offset;
            }
          else
            {
              code += 4;
              code2 = fx_writecases(code+6, lastcmp, fxc, fixtargets,
                                    first, middle, supposed_end);
              code[-6] = 0x0F;
              code[-5] = 0x8F;    /* JG rel32 */
              *(long*)(code-4) = code2 - code;
            }
        }
      else
        code2 = code+6;
      code[0] = 0x0F;
      code[1] = 0x84;    /* JE rel32 */
      code += 6;
      fixtargets[fxc[middle].index] = *(long*)(code-4) = (supposed_end+5) - code;
      code = fx_writecases(code2, lastcmp, fxc, fixtargets,
                           middle+1, last, supposed_end);
    }
  return code;
}

/* preparation for psyco_write_run_time_switch() */
DEFINEFN
int psyco_build_run_time_switch(fixed_switch_t* rts, long kflags,
				long values[], int count)
{
  code_t* code;
  code_t* codeend;
  code_t* codeorigin = NULL;
  struct fxcase_s* fxc = NULL;
  long* fixtargets = NULL;
  int i, size;
  
    /* a large enough buffer, will be shrunk later */
  codeorigin = (code_t*) PyMem_MALLOC(FX_BASE_SIZE + count * FX_MAX_ITEM_SIZE);
  if (codeorigin == NULL)
    goto out_of_memory;
  fxc = (struct fxcase_s*) PyMem_MALLOC(count *
                                         (sizeof(struct fxcase_s)+sizeof(long)));
  if (fxc == NULL)
    goto out_of_memory;
  /* 'fixtargets' lists the offset of the targets to fix in 'switchcode'.
     It is not stored in the array 'fxc' because it is indexed by the
     original index value, whereas 'fxc' is sorted by value. But we can
     put 'fixtargets' after 'fxc' in the same memory block. */
  fixtargets = (long*) (fxc+count);
  
  for (i=0; i<count; i++)
    {
      fxc[i].value = values[i];
      fxc[i].index = i;
    }
  qsort(fxc, count, sizeof(struct fxcase_s), &fx_compare);

  /* we try to emit the code by supposing that its end is at 'codeend'.
     Depending on the supposition this creates either short or long jumps,
     so it changes the size of the code. We begin by supposing that the
     end is at the beginning, and emit code over and over again until the
     real end stabilizes at the presupposed end position. (It can be proved
     to converge.) */
  codeend = codeorigin;
  while (1)
    {
      code_t* lastcmp = NULL;
      code = fx_writecases(codeorigin, &lastcmp, fxc, fixtargets,
                           0, count, codeend);
      if (lastcmp != NULL)
        *lastcmp = 0;  /* end of list */
      if (code == (codeend + FX_LAST_JUMP_SIZE))
        break;   /* ok, it converged */
      codeend = code - FX_LAST_JUMP_SIZE;   /* otherwise try again */
    }
  /* the LAST_JUMP is a 'JMP rel8', which we do not need (it corresponds
     to the default case jumping to the supposed_end). We overwrite it
     with a 'JMP rel32' which jumps at supposed_end+5, that it,
     at the end of the block. */
  codeend[0] = 0xE9;    /* JMP rel32 */
  codeend += 5;
  *(long*)(codeend-4) = 0;

  size = codeend - codeorigin;
  codeorigin = (code_t*) PyMem_REALLOC(codeorigin, size);
  if (codeorigin == NULL)
    goto out_of_memory;

  rts->switchcodesize = size;
  rts->switchcode = codeorigin;
  rts->count = count;
  rts->fxc = fxc;
  rts->fixtargets = fixtargets;
  rts->zero = 0;
  rts->fixed_promotion.header.compute_fn = &computed_promotion;
  rts->fixed_promotion.fs = rts;
  rts->fixed_promotion.kflags = kflags;
  return 0;

 out_of_memory:
  PyMem_FREE(fxc);
  PyMem_FREE(codeorigin);
  PyErr_NoMemory();
  return -1;
}

DEFINEFN
int psyco_switch_lookup(fixed_switch_t* rts, long value)
{
  /* look-up in the fxc array */
  struct fxcase_s* fxc = rts->fxc;
  int first = 0, last = rts->count;
  while (first < last)
    {
      int middle = (first+last)/2;
      if (fxc[middle].value == value)
        return fxc[middle].index;   /* found, return index */
      if (fxc[middle].value < value)
        first = middle+1;
      else
        last = middle;
    }
  return -1;    /* not found */
}

DEFINEFN
code_t* psyco_write_run_time_switch(fixed_switch_t* rts, code_t* code, reg_t reg)
{
  /* Write the code that does a 'switch' on the prepared 'values'. */
  memcpy(code, rts->switchcode, rts->switchcodesize);

  if (rts->count > 0)
    {
      /* Fix the 1st operand (register number) used by all CMP instruction */
      code_t new_value = 0xC0 | (7<<3) | reg;
      code_t* fix = code+1;   /* 2nd byte of first CMP instruction */
      while (1)
        {
          int offset = *fix;
          *fix = new_value;
          if (offset == 0)
            break;
          fix += offset;
        }
    }
  return code + rts->switchcodesize;
}

DEFINEFN
void psyco_fix_switch_case(fixed_switch_t* rts, code_t* code,
                           int item, code_t* newtarget)
{
/* Fix the target corresponding to the given case. */
  long fixtarget = item<0 ? 0 : rts->fixtargets[item];
  code_t* fixme = code - fixtarget;
  *(long*)(fixme-4) = newtarget - fixme;
}

#endif   /* USE_RUNTIME_SWITCHES */


/*****************************************************************/
 /***   Calling C functions                                     ***/

#define MAX_ARGUMENTS_COUNT    16

DEFINEFN
vinfo_t* psyco_generic_call(PsycoObject* po, void* c_function,
                            int flags, const char* arguments, ...)
{
	char argtags[MAX_ARGUMENTS_COUNT];
	long raw_args[MAX_ARGUMENTS_COUNT], args[MAX_ARGUMENTS_COUNT];
	int count, i, j, stackbase, totalstackspace = 0;
	vinfo_t* vresult;
	bool has_refs = false;

	va_list vargs;

#ifdef HAVE_STDARG_PROTOTYPES
	va_start(vargs, arguments);
#else
	va_start(vargs);
#endif
	extra_assert(c_function != NULL);

	for (count=0; arguments[count]; count++) {
		long arg;
		char tag;
		vinfo_t* vi;
		
		extra_assert(count <= MAX_ARGUMENTS_COUNT);
		raw_args[count] = arg = va_arg(vargs, long);
		tag = arguments[count];

		switch (tag) {
			
		case 'l':
			break;
			
		case 'v':
			/* Compute all values first */
			vi = (vinfo_t*) arg;
			if (!compute_vinfo(vi, po)) return NULL;
			if (!is_compiletime(vi->source)) {
				flags &= ~CfPure;
			}
			else {
				/* compile-time: get the value */
				arg = CompileTime_Get(vi->source)->value;
				tag = 'l';
			}
			break;

		case 'r':
			/* Push by-reference values in the stack now */
			vi = (vinfo_t*) arg;
			extra_assert(is_runtime(vi->source));
			if (getstack(vi->source) == RunTime_StackNone) {
				reg_t rg = getreg(vi->source);
				if (rg == REG_NONE) {
					/* for undefined sources, pushing
					   just any register will be fine */
					rg = REG_ANY_CALLER_SAVED;
				}
				BEGIN_CODE
				SAVE_REG_VINFO(vi, rg);
				END_CODE
			}
                        arg = RunTime_NewStack(getstack(vi->source),
                                               REG_NONE, false, false);
			has_refs = true;
			break;

		case 'a':
		case 'A':
			has_refs = true;
			totalstackspace += 4*((vinfo_array_t*) arg)->count;
			break;

		default:
			Py_FatalError("unknown character argument in"
				      " psyco_generic_call()");
		}
		args[count] = arg;
		argtags[count] = tag;
	}
	va_end(vargs);

        if (flags & CfPure) {
                /* calling a pure function with no run-time argument */
                long result;

                if (has_refs) {
                    for (i = 0; i < count; i++) {
                        if (argtags[i] == 'a' || argtags[i] == 'A') {
							int cnt = ((vinfo_array_t*)args[i])->count;
                            args[i] = (long)malloc(cnt*sizeof(long));
                        }
#if ALL_CHECKS
                        if (argtags[i] == 'r')
                            Py_FatalError("psyco_generic_call(): arg mode "
                            "incompatible with CfPure");
                        
#endif
                    }
                }
                result = psyco_call_var(c_function, count, args);
                if (PyErr_Occurred()) {
                    if (has_refs)
                        for (i = 0; i < count; i++) 
                            if (argtags[i] == 'a' || argtags[i] == 'A')
                                free((void*)args[i]);
                    psyco_virtualize_exception(po);
                    return NULL;
                }
                if (has_refs) {
                    for (i = 0; i < count; i++)
                        if (argtags[i] == 'a' || argtags[i] == 'A') {
                            vinfo_array_t* array = (vinfo_array_t*)raw_args[i];
                            long sk_flag = (argtags[i] == 'a') ? 0 : SkFlagPyObj;
                            for (j = 0; j < array->count; j++) {
                                array->items[j] = vinfo_new(CompileTime_NewSk(
                                    sk_new( ((long*)args[i])[j], sk_flag)));
                            }
                            free((void*)args[i]);
                        }
                }

		if (flags & CfPyErrMask) {
			/* such functions are rarely pure, but there are
			   exceptions with CfPyErrNotImplemented */
			vresult = generic_call_ct(flags, result);
			if (vresult != NULL)
				return vresult;
		}
		
		switch (flags & CfReturnMask) {

		case CfReturnNormal:
			vresult = vinfo_new(CompileTime_New(result));
			break;
			
		case CfReturnRef:
			vresult = vinfo_new(CompileTime_NewSk(sk_new(result,
								SkFlagPyObj)));
			break;

		default:
			vresult = (vinfo_t*) 1;   /* anything non-NULL */
		}
		return vresult;
	}

	if (has_refs) {
		/* we will need a trash register to compute the references
		   we push later. The following three lines prevent another
		   argument which would currently be in the same trash
		   register from being pushed from the register after we
		   clobbered it. */
		BEGIN_CODE
		NEED_REGISTER(REG_ANY_CALLER_SAVED);
		END_CODE
	}
	
	for (count=0; arguments[count]; count++) {
		if (argtags[count] == 'v') {
			/* We collect all the sources in 'args' now,
			   before SAVE_REGS_FN_CALLS which might move
			   some run-time values into the stack. In this
			   case the old copy in the registers is still
			   useable to PUSH it for the C function call. */
			RunTimeSource src = ((vinfo_t*)(args[count]))->source;
			args[count] = (long) src;
		}
	}

	BEGIN_CODE
	SAVE_REGS_FN_CALLS;
	stackbase = po->stack_depth;
	po->stack_depth += totalstackspace;
	STACK_CORRECTION(totalstackspace);
	for (i=count; i--; ) {
		switch (argtags[i]) {
			
		case 'v':
			CALL_SET_ARG_FROM_RT (args[i],   i, count);
			break;
			
		case 'r':
			LOAD_ADDRESS_FROM_RT (args[i],   REG_ANY_CALLER_SAVED);
			CALL_SET_ARG_FROM_RT (RunTime_New(REG_ANY_CALLER_SAVED,
						     false, false),  i, count);
			break;
			
		case 'a':
		case 'A':
		{
			vinfo_array_t* array = (vinfo_array_t*) args[i];
			bool with_reference = (argtags[i] == 'A');
			int j = array->count;
			if (j > 0) {
				do {
					stackbase += 4;
					array->items[--j] = vinfo_new
					    (RunTime_NewStack(stackbase,
					       REG_NONE, with_reference, false));
				} while (j);
				LOAD_ADDRESS_FROM_RT (array->items[0]->source,
						      REG_ANY_CALLER_SAVED);
			}
			CALL_SET_ARG_FROM_RT (RunTime_New(REG_ANY_CALLER_SAVED,
						       false, false),  i, count);
			break;
		}
			
		default:
			CALL_SET_ARG_IMMED   (args[i],   i, count);
			break;
		}
	}
	CALL_C_FUNCTION                      (c_function,   count);
	END_CODE

	switch (flags & CfReturnMask) {

	case CfReturnNormal:
		vresult = new_rtvinfo(po, REG_FUNCTIONS_RETURN, false, false);
		break;

	case CfReturnRef:
		vresult = new_rtvinfo(po, REG_FUNCTIONS_RETURN, true, false);
		break;

	default:
		if ((flags & CfPyErrMask) == 0)
			return (vinfo_t*) 1;   /* anything non-NULL */
		
		vresult = new_rtvinfo(po, REG_FUNCTIONS_RETURN, false, false);
		vresult = generic_call_check(po, flags, vresult);
		if (vresult == NULL)
			goto error_detected;
		vinfo_decref(vresult, po);
		return (vinfo_t*) 1;   /* anything non-NULL */
	}
	
        if (flags & CfPyErrMask) {
		vresult = generic_call_check(po, flags, vresult);
		if (vresult == NULL)
			goto error_detected;
	}
	return vresult;

   error_detected:
	/* if the called function returns an error, we then assume that
	   it did not actually fill the arrays */
	if (has_refs) {
		for (i = 0; i < count; i++)
			if (argtags[i] == 'a' || argtags[i] == 'A') {
				vinfo_array_t* array = (vinfo_array_t*)args[i];
				int j = array->count;
				while (j--) {
					vinfo_t* v = array->items[j];
					array->items[j] = NULL;
					v->source = remove_rtref(v->source);
					vinfo_decref(v, po);
				}
                        }
	}
	return NULL;
}


DEFINEFN
vinfo_t* psyco_call_psyco(PsycoObject* po, CodeBufferObject* codebuf,
			  Source argsources[], int argcount,
			  struct stack_frame_info_s* finfo)
{
	/* this is a simplified version of psyco_generic_call() which
	   assumes Psyco's calling convention instead of the C's. */
	int i, initial_depth;
	bool ccflags;
	Source* p;
	BEGIN_CODE
          /* cannot use NEED_CC() */
	ccflags = (po->ccreg != NULL);
	if (ccflags)
		PUSH_CC_FLAGS();
	for (i=0; i<REG_TOTAL; i++)
		NEED_REGISTER(i);
	finfo->stack_depth = po->stack_depth;
	SAVE_IMMED_TO_EBP_BASE((long) finfo, INITIAL_STACK_DEPTH);
	initial_depth = po->stack_depth;
	CALL_SET_ARG_IMMED(-1, argcount, argcount+1);
	p = argsources;
	for (i=argcount; i--; p++)
		CALL_SET_ARG_FROM_RT(*p, i, argcount+1);
	CALL_C_FUNCTION(codebuf->codestart, argcount+1);
	po->stack_depth = initial_depth;  /* callee removes arguments */
	SAVE_IMM8_TO_EBP_BASE(-1, INITIAL_STACK_DEPTH);
	if (ccflags)
		POP_CC_FLAGS();
	END_CODE
	return generic_call_check(po, CfReturnRef|CfPyErrIfNull,
			   new_rtvinfo(po, REG_FUNCTIONS_RETURN, true, false));
}

DEFINEFN struct stack_frame_info_s**
psyco_next_stack_frame(struct stack_frame_info_s** finfo)
{
	/* Hack to pick directly from the machine stack the stored
	   "stack_frame_info_t*" pointers */
	return (struct stack_frame_info_s**)
		(((char*) finfo) - (*finfo)->stack_depth);
}


/*****************************************************************/
 /***   Emit common instructions                                ***/

DEFINEFN
condition_code_t integer_non_null(PsycoObject* po, vinfo_t* vi)
{
	condition_code_t result;
	
	if (is_virtualtime(vi->source)) {
		result = psyco_vsource_cc(vi->source);
		if (result != CC_ALWAYS_FALSE)
			return result;
		if (!compute_vinfo(vi, po))
			return CC_ERROR;
	}
	if (is_compiletime(vi->source)) {
		if (KNOWN_SOURCE(vi)->value != 0)
			return CC_ALWAYS_TRUE;
		else
			return CC_ALWAYS_FALSE;
	}
	BEGIN_CODE
	CHECK_NONZERO_FROM_RT(vi->source);
	END_CODE
	return CHECK_NONZERO_CONDITION;
}

DEFINEFN
condition_code_t integer_NON_NULL(PsycoObject* po, vinfo_t* vi)
{
	condition_code_t result;

	if (vi == NULL)
		return CC_ERROR;

        result = integer_non_null(po, vi);

	/* 'vi' cannot be a reference to a Python object if we are
	   asking ourselves if it is NULL or not. So the following
	   vinfo_decref() will not emit a Py_DECREF() that would
	   clobber the condition code. We check all this. */
#if ALL_CHECKS
	extra_assert(!has_rtref(vi->source));
	{ code_t* code1 = po->code;
#endif
	vinfo_decref(vi, po);
#if ALL_CHECKS
	extra_assert(po->code == code1); }
#endif
	return result;
}

#define GENERIC_BINARY_HEADER                   \
  if (!compute_vinfo(v2, po) || !compute_vinfo(v1, po)) return NULL;

#define GENERIC_BINARY_HEADER_i                 \
  if (!compute_vinfo(v1, po)) return NULL;

#define GENERIC_BINARY_CT_CT(c_code)                            \
  if (is_compiletime(v1->source) && is_compiletime(v2->source)) \
{                                                               \
      long a = CompileTime_Get(v1->source)->value;              \
      long b = CompileTime_Get(v2->source)->value;              \
      long c = (c_code);                                        \
      return vinfo_new(CompileTime_New(c));                     \
    }

#define GENERIC_BINARY_COMMON_INSTR(group, ovf, nonneg)   {     \
  reg_t rg;                                                     \
  BEGIN_CODE                                                    \
  NEED_CC();                                                    \
  COPY_IN_REG(v1, rg);                      /* MOV rg, (v1) */  \
  COMMON_INSTR_FROM(group, rg, v2->source); /* XXX rg, (v2) */  \
  END_CODE                                                      \
  if ((ovf) && runtime_condition_f(po, CC_O))                   \
    return NULL;  /* if overflow */                             \
  return new_rtvinfo(po, rg, false, nonneg);                    \
}

#define GENERIC_BINARY_INSTR_2(group, c_code, nonneg)                   \
{                                                                       \
  if (!compute_vinfo(v1, po)) return NULL;                              \
  if (is_compiletime(v1->source))                                       \
    {                                                                   \
      long a = CompileTime_Get(v1->source)->value;                      \
      long b = value2;                                                  \
      long c = (c_code);                                                \
      return vinfo_new(CompileTime_New(c));                             \
    }                                                                   \
  else                                                                  \
    {                                                                   \
      reg_t rg;                                                         \
      BEGIN_CODE                                                        \
      NEED_CC();                                                        \
      COPY_IN_REG(v1, rg);                   /* MOV rg, (v1) */         \
      COMMON_INSTR_IMMED(group, rg, value2); /* XXX rg, value2 */       \
      END_CODE                                                          \
      return new_rtvinfo(po, rg, false, nonneg);                        \
    }                                                                   \
}

static vinfo_t* int_add_i(PsycoObject* po, vinfo_t* rt1, long value2,
                          bool unsafe)
{
  reg_t rg, dst;
  extra_assert(is_runtime(rt1->source));
  BEGIN_CODE
  NEED_FREE_REG(dst);
  rg = getreg(rt1->source);
  if (rg == REG_NONE)
    {
      rg = dst;
      LOAD_REG_FROM(rt1->source, rg);
    }
  LOAD_REG_FROM_REG_PLUS_IMMED(dst, rg, value2);
  END_CODE
  return new_rtvinfo(po, dst, false,
		unsafe && value2>=0 && is_rtnonneg(rt1->source));
}

DEFINEFN
vinfo_t* integer_add(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf)
{
  GENERIC_BINARY_HEADER
  if (is_compiletime(v1->source))
    {
      long a = CompileTime_Get(v1->source)->value;
      if (a == 0)
        {
          /* adding zero to v2 */
          vinfo_incref(v2);
          return v2;
        }
      if (is_compiletime(v2->source))
        {
          long b = CompileTime_Get(v2->source)->value;
          long c = a + b;
          if (ovf && (c^a) < 0 && (c^b) < 0)
            return NULL;   /* overflow */
          return vinfo_new(CompileTime_New(c));
        }
      if (!ovf)
        return int_add_i(po, v2, a, false);
    }
  else
    if (is_compiletime(v2->source))
      {
        long b = CompileTime_Get(v2->source)->value;
        if (b == 0)
          {
            /* adding zero to v1 */
            vinfo_incref(v1);
            return v1;
          }
        if (!ovf)
          return int_add_i(po, v1, b, false);
      }

  GENERIC_BINARY_COMMON_INSTR(0, ovf,   /* ADD */
			      ovf && is_nonneg(v1->source)
			          && is_nonneg(v2->source))
}

DEFINEFN
vinfo_t* integer_add_i(PsycoObject* po, vinfo_t* v1, long value2, bool unsafe)
{
  if (value2 == 0)
    {
      /* adding zero to v1 */
      vinfo_incref(v1);
      return v1;
    }
  else
    {
      GENERIC_BINARY_HEADER_i
      if (is_compiletime(v1->source))
        {
          long c = CompileTime_Get(v1->source)->value + value2;
          return vinfo_new(CompileTime_New(c));
        }
      return int_add_i(po, v1, value2, unsafe);
    }
}

DEFINEFN
vinfo_t* integer_sub(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf)
{
  GENERIC_BINARY_HEADER
  if (is_compiletime(v1->source))
    {
      long a = CompileTime_Get(v1->source)->value;
      if (is_compiletime(v2->source))
        {
          long b = CompileTime_Get(v2->source)->value;
          long c = a - b;
          if (ovf && (c^a) < 0 && (c^~b) < 0)
            return NULL;   /* overflow */
          return vinfo_new(CompileTime_New(c));
        }
    }
  else
    if (is_compiletime(v2->source))
      {
        long b = CompileTime_Get(v2->source)->value;
        if (b == 0)
          {
            /* subtracting zero from v1 */
            vinfo_incref(v1);
            return v1;
          }
        if (!ovf)
          return int_add_i(po, v1, -b, false);
      }

  GENERIC_BINARY_COMMON_INSTR(5, ovf, false)   /* SUB */
}

DEFINEFN
vinfo_t* integer_or(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  GENERIC_BINARY_HEADER
  GENERIC_BINARY_CT_CT(a | b)
  GENERIC_BINARY_COMMON_INSTR(1, false,   /* OR */
			      is_nonneg(v1->source) &&
			      is_nonneg(v2->source));
}

DEFINEFN
vinfo_t* integer_xor(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  GENERIC_BINARY_HEADER
  GENERIC_BINARY_CT_CT(a ^ b)
  GENERIC_BINARY_COMMON_INSTR(6, false,   /* XOR */
			      is_nonneg(v1->source) &&
			      is_nonneg(v2->source));
}

DEFINEFN
vinfo_t* integer_and(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  GENERIC_BINARY_HEADER
  GENERIC_BINARY_CT_CT(a & b)
  GENERIC_BINARY_COMMON_INSTR(4, false,   /* AND */
			      is_nonneg(v1->source) ||
			      is_nonneg(v2->source));
}

#if 0
DEFINEFN   -- unused --
vinfo_t* integer_and_i(PsycoObject* po, vinfo_t* v1, long value2)
     GENERIC_BINARY_INSTR_2(4, a & b,    /* AND */
			    value2>=0 || is_rtnonneg(v1->source))
#endif

#define GENERIC_SHIFT_BY(rtmacro, nonneg)               \
  {                                                     \
    reg_t rg;                                           \
    extra_assert(0 < counter && counter < LONG_BIT);    \
    BEGIN_CODE                                          \
    NEED_CC();                                          \
    COPY_IN_REG(v1, rg);                                \
    rtmacro(rg, counter);                               \
    END_CODE                                            \
    return new_rtvinfo(po, rg, false, nonneg);          \
  }
     
#define GENERIC_RT_SHIFT(rtmacro, nonneg)       \
  {                                             \
    reg_t rg;                                   \
    BEGIN_CODE                                  \
    if (RSOURCE_REG(v2->source) != SHIFT_COUNTER) { \
      NEED_REGISTER(SHIFT_COUNTER);             \
      LOAD_REG_FROM(v2->source, SHIFT_COUNTER); \
    }                                           \
    NEED_CC_REG(SHIFT_COUNTER);                 \
    DELAY_USE_OF(SHIFT_COUNTER);                \
    COPY_IN_REG(v1, rg);                        \
    rtmacro(rg);      /* XXX rg, CL */          \
    END_CODE                                    \
    return new_rtvinfo(po, rg, false, nonneg);  \
  }


static vinfo_t* int_lshift_i(PsycoObject* po, vinfo_t* v1, int counter)
     GENERIC_SHIFT_BY(SHIFT_LEFT_BY, false)

static vinfo_t* int_rshift_i(PsycoObject* po, vinfo_t* v1, int counter)
     GENERIC_SHIFT_BY(SHIFT_SIGNED_RIGHT_BY, is_nonneg(v1->source))

static vinfo_t* int_urshift_i(PsycoObject* po, vinfo_t* v1, int counter)
     GENERIC_SHIFT_BY(SHIFT_RIGHT_BY, true)

static vinfo_t* int_mul_i(PsycoObject* po, vinfo_t* v1, long value2,
                          bool ovf)
{
  switch (value2) {
  case 0:
    return vinfo_new(CompileTime_New(0));
  case 1:
    vinfo_incref(v1);
    return v1;
  }
  if (((value2-1) & value2) == 0 && value2 >= 0 && !ovf)
    {
      /* value2 is a power of two */
      return int_lshift_i(po, v1, intlog2(value2));
    }
  else
    {
      reg_t rg;
      BEGIN_CODE
      NEED_CC();
      NEED_FREE_REG(rg);
      IMUL_IMMED_FROM_RT(v1->source, value2, rg);
      END_CODE
      if (ovf && runtime_condition_f(po, CC_O))
        return NULL;
      return new_rtvinfo(po, rg, false,
	      ovf && value2>=0 && is_rtnonneg(v1->source));
    }
}

DEFINEFN
vinfo_t* integer_mul(PsycoObject* po, vinfo_t* v1, vinfo_t* v2, bool ovf)
{
  reg_t rg;
  GENERIC_BINARY_HEADER
  if (is_compiletime(v1->source))
    {
      long a = CompileTime_Get(v1->source)->value;
      if (is_compiletime(v2->source))
        {
          long b = CompileTime_Get(v2->source)->value;
          /* unlike Python, we use a function written in assembly
             to perform the product overflow checking */
          if (ovf && glue_int_mul_1(a, b))
            return NULL;   /* overflow */
          return vinfo_new(CompileTime_New(a * b));
        }
      return int_mul_i(po, v2, a, ovf);
    }
  else
    if (is_compiletime(v2->source))
      {
        long b = CompileTime_Get(v2->source)->value;
        return int_mul_i(po, v1, b, ovf);
      }
  
  BEGIN_CODE
  NEED_CC();
  COPY_IN_REG(v1, rg);               /* MOV rg, (v1) */
  IMUL_REG_FROM_RT(v2->source, rg);  /* IMUL rg, (v2) */
  END_CODE
  if (ovf && runtime_condition_f(po, CC_O))
    return NULL;  /* if overflow */
  return new_rtvinfo(po, rg, false,
	  ovf && is_rtnonneg(v1->source) && is_rtnonneg(v2->source));
}

DEFINEFN
vinfo_t* integer_mul_i(PsycoObject* po, vinfo_t* v1, long value2)
{
  GENERIC_BINARY_HEADER_i
  if (is_compiletime(v1->source))
    {
      long c = CompileTime_Get(v1->source)->value * value2;
      return vinfo_new(CompileTime_New(c));
    }
  return int_mul_i(po, v1, value2, false);
}

/* forward */
static condition_code_t int_cmp_i(PsycoObject* po, vinfo_t* rt1,
                                  long immed2, condition_code_t result);

DEFINEFN
vinfo_t* integer_lshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  condition_code_t cc;
  GENERIC_BINARY_HEADER
  if (is_compiletime(v2->source))
    return integer_lshift_i(po, v1, CompileTime_Get(v2->source)->value);

  cc = int_cmp_i(po, v2, LONG_BIT, CC_uGE);
  if (cc == CC_ERROR)
    return NULL;

  if (runtime_condition_f(po, cc))
    {
      cc = int_cmp_i(po, v2, 0, CC_L);
      if (cc == CC_ERROR)
        return NULL;
      if (runtime_condition_f(po, cc))
        {
          PycException_SetString(po, PyExc_ValueError, "negative shift count");
          return NULL;
        }
      return vinfo_new(CompileTime_New(0));
    }
  GENERIC_RT_SHIFT(SHIFT_LEFT_CL, false);
}

DEFINEFN
vinfo_t* integer_lshift_i(PsycoObject* po, vinfo_t* v1, long counter)
{
  GENERIC_BINARY_HEADER_i
  if (0 < counter && counter < LONG_BIT)
    {
      if (is_compiletime(v1->source))
        {
          long c = CompileTime_Get(v1->source)->value << counter;
          return vinfo_new(CompileTime_New(c));
        }
      else
        return int_lshift_i(po, v1, counter);
    }
  else if (counter == 0)
    {
      vinfo_incref(v1);
      return v1;
    }
  else if (counter >= LONG_BIT)
    return vinfo_new(CompileTime_New(0));
  else
    {
      PycException_SetString(po, PyExc_ValueError, "negative shift count");
      return NULL;
    }
}

DEFINEFN      /* signed */
vinfo_t* integer_rshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2)
{
  condition_code_t cc;
  GENERIC_BINARY_HEADER
  if (is_compiletime(v2->source))
    return integer_rshift_i(po, v1, CompileTime_Get(v2->source)->value);

  cc = int_cmp_i(po, v2, LONG_BIT, CC_uGE);
  if (cc == CC_ERROR)
    return NULL;

  if (runtime_condition_f(po, cc))
    {
      cc = int_cmp_i(po, v2, 0, CC_L);
      if (cc == CC_ERROR)
        return NULL;
      if (runtime_condition_f(po, cc))
        {
          PycException_SetString(po, PyExc_ValueError, "negative shift count");
          return NULL;
        }
      return integer_rshift_i(po, v1, LONG_BIT-1);
    }
  GENERIC_RT_SHIFT(SHIFT_SIGNED_RIGHT_CL, false);
}

DEFINEFN      /* signed */
vinfo_t* integer_rshift_i(PsycoObject* po, vinfo_t* v1, long counter)
{
  GENERIC_BINARY_HEADER_i
  if (counter >= LONG_BIT-1)
    {
      if (is_nonneg(v1->source))
        return vinfo_new(CompileTime_New(0));
      counter = LONG_BIT-1;
    }
  if (counter > 0)
    {
      if (is_compiletime(v1->source))
        {
          long c = ((long)(CompileTime_Get(v1->source)->value)) >> counter;
          return vinfo_new(CompileTime_New(c));
        }
      else
        return int_rshift_i(po, v1, counter);
    }
  else if (counter == 0)
    {
      vinfo_incref(v1);
      return v1;
    }
  else
    {
      PycException_SetString(po, PyExc_ValueError, "negative shift count");
      return NULL;
    }
}

DEFINEFN
vinfo_t* integer_urshift_i(PsycoObject* po, vinfo_t* v1, long counter)
{
  GENERIC_BINARY_HEADER_i
  if (0 < counter && counter < LONG_BIT)
    {
      if (is_compiletime(v1->source))
        {
          long c = ((unsigned long)(CompileTime_Get(v1->source)->value)) >> counter;
          return vinfo_new(CompileTime_New(c));
        }
      else
        return int_urshift_i(po, v1, counter);
    }
  else if (counter == 0)
    {
      vinfo_incref(v1);
      return v1;
    }
  else if (counter >= LONG_BIT)
    return vinfo_new(CompileTime_New(0));
  else
    {
      PycException_SetString(po, PyExc_ValueError, "negative shift count");
      return NULL;
    }
}

/* DEFINEFN */
/* vinfo_t* integer_lshift(PsycoObject* po, vinfo_t* v1, vinfo_t* v2) */
/* { */
/*   NonVirtualSource v1s, v2s; */
/*   v2s = vinfo_compute(v2, po); */
/*   if (v2s == SOURCE_ERROR) return NULL; */
/*   if (is_compiletime(v2s)) */
/*     return integer_lshift_i(po, v1, CompileTime_Get(v2s)->value); */
  
/*   v1s = vinfo_compute(v1, po); */
/*   if (v1s == SOURCE_ERROR) return NULL; */
/*   XXX implement me */
/* } */


#define GENERIC_UNARY_INSTR(rtmacro, c_code, ovf, c_ovf, cond_ovf, nonneg, XTR) \
{                                                                       \
  if (!compute_vinfo(v1, po)) return NULL;                              \
  XTR                                                                   \
  if (is_compiletime(v1->source))                                       \
    {                                                                   \
      long a = CompileTime_Get(v1->source)->value;                      \
      long c = (c_code);                                                \
      if (!((ovf) && (c_ovf)))                                          \
        return vinfo_new(CompileTime_New(c));                           \
    }                                                                   \
  else                                                                  \
    {                                                                   \
      reg_t rg;                                                         \
      BEGIN_CODE                                                        \
      NEED_CC();                                                        \
      COPY_IN_REG(v1, rg);                  /* MOV rg, (v1) */          \
      rtmacro;                              /* XXX rg       */          \
      END_CODE                                                          \
      if (!((ovf) && runtime_condition_f(po, cond_ovf)))                \
        return new_rtvinfo(po, rg, false, nonneg);                      \
    }                                                                   \
  return NULL;                                                          \
}

DEFINEFN
vinfo_t* integer_not(PsycoObject* po, vinfo_t* v1)
  GENERIC_UNARY_INSTR(UNARY_INSTR_ON_REG(2, rg), ~a,
                           false, false, CC_ALWAYS_FALSE, false, ;)

DEFINEFN
vinfo_t* integer_neg(PsycoObject* po, vinfo_t* v1, bool ovf)
  GENERIC_UNARY_INSTR(UNARY_INSTR_ON_REG(3, rg), -a,
                           ovf, c == (-LONG_MAX-1), CC_O, false, ;)

DEFINEFN
vinfo_t* integer_abs(PsycoObject* po, vinfo_t* v1, bool ovf)
  GENERIC_UNARY_INSTR(INT_ABS(rg, v1->source), a<0 ? -a : a,
                           ovf, c == (-LONG_MAX-1), CHECK_ABS_OVERFLOW,
                           /*ovf*/ true  /* assert no overflow */,
                      if (is_nonneg(v1->source)) { vinfo_incref(v1); return v1; })

static const condition_code_t direct_results[16] = {
	  /*****   signed comparison      **/
          /* Py_LT: */  CC_L,
          /* Py_LE: */  CC_LE,
          /* Py_EQ: */  CC_E,
          /* Py_NE: */  CC_NE,
          /* Py_GT: */  CC_G,
          /* Py_GE: */  CC_GE,
	  /* (6)    */  CC_ERROR,
	  /* (7)    */  CC_ERROR,
	  /*****  unsigned comparison     **/
          /* Py_LT: */  CC_uL,
          /* Py_LE: */  CC_uLE,
          /* Py_EQ: */  CC_E,
          /* Py_NE: */  CC_NE,
          /* Py_GT: */  CC_uG,
          /* Py_GE: */  CC_uGE,
	  /* (14)   */  CC_ERROR,
	  /* (15)   */  CC_ERROR };

static const condition_code_t inverted_results[16] = {
	  /*****   signed comparison      **/
          /* inverted Py_LT: */  CC_G,
          /* inverted Py_LE: */  CC_GE,
          /* inverted Py_EQ: */  CC_E,
          /* inverted Py_NE: */  CC_NE,
          /* inverted Py_GT: */  CC_L,
          /* inverted Py_GE: */  CC_LE,
	  /* (6)             */  CC_ERROR,
	  /* (7)             */  CC_ERROR,
	  /*****  unsigned comparison     **/
          /* inverted Py_LT: */  CC_uG,
          /* inverted Py_LE: */  CC_uGE,
          /* inverted Py_EQ: */  CC_E,
          /* inverted Py_NE: */  CC_NE,
          /* inverted Py_GT: */  CC_uL,
          /* inverted Py_GE: */  CC_uLE,
	  /* (14)            */  CC_ERROR,
	  /* (15)            */  CC_ERROR };

static condition_code_t immediate_compare(long a, long b, int py_op)
{
  switch (py_op) {
    case Py_LT:  return a < b  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_LE:  return a <= b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_EQ|COMPARE_UNSIGNED:
    case Py_EQ:  return a == b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_NE|COMPARE_UNSIGNED:
    case Py_NE:  return a != b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_GT:  return a > b  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
    case Py_GE:  return a >= b ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;

  case Py_LT|COMPARE_UNSIGNED:  return ((unsigned long) a) <  ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  case Py_LE|COMPARE_UNSIGNED:  return ((unsigned long) a) <= ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  case Py_GT|COMPARE_UNSIGNED:  return ((unsigned long) a) >  ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  case Py_GE|COMPARE_UNSIGNED:  return ((unsigned long) a) >= ((unsigned long) b)
                                  ? CC_ALWAYS_TRUE : CC_ALWAYS_FALSE;
  default:
    Py_FatalError("immediate_compare(): bad py_op");
    return CC_ERROR;
  }
}

static condition_code_t int_cmp_i(PsycoObject* po, vinfo_t* rt1,
                                  long immed2, condition_code_t result)
{
  extra_assert(is_runtime(rt1->source));
  /* detect easy cases */
  if (immed2 == 0)
    {
      if (is_rtnonneg(rt1->source))   /* if we know that rt1>=0 */
        switch (result) {
        case CC_L:   /* rt1 < 0 */
          return CC_ALWAYS_FALSE;
        case CC_GE:  /* rt1 >= 0 */
          return CC_ALWAYS_TRUE;
        default:
          ; /* pass */
        }
      switch (result) {
      case CC_uL:   /* (unsigned) rt1 < 0 */
        return CC_ALWAYS_FALSE;
      case CC_uGE:  /* (unsigned) rt1 >= 0 */
        return CC_ALWAYS_TRUE;
      default:
        ; /* pass */
      }
    }
  else if (immed2 < 0 && is_rtnonneg(rt1->source))  /* rt1>=0 && immed2<0 */
    {
      switch (result) {
      case CC_E:    /* rt1 == immed2 */
      case CC_L:    /* rt1 <  immed2 */
      case CC_LE:   /* rt1 <= immed2 */
      case CC_uG:   /* (unsigned) rt1 >  immed2 */
      case CC_uGE:  /* (unsigned) rt1 >= immed2 */
        return CC_ALWAYS_FALSE;
      case CC_NE:   /* rt1 != immed2 */
      case CC_GE:   /* rt1 >= immed2 */
      case CC_G:    /* rt1 >  immed2 */
      case CC_uLE:  /* (unsigned) rt1 <= immed2 */
      case CC_uL:   /* (unsigned) rt1 <  immed2 */
        return CC_ALWAYS_TRUE;
      default:
        ; /* pass */
      }
    }
  /* end of easy cases */
  BEGIN_CODE
  NEED_CC();
  COMPARE_IMMED_FROM_RT(rt1->source, immed2); /* CMP v1, immed2 */
  END_CODE
  return result;
}

DEFINEFN
condition_code_t integer_cmp(PsycoObject* po, vinfo_t* v1,
                             vinfo_t* v2, int py_op)
{
  condition_code_t result;

  if (vinfo_known_equal(v1, v2))
    goto same_source;

  if (!compute_vinfo(v1, po) || !compute_vinfo(v2, po))
    return CC_ERROR;

  if (vinfo_known_equal(v1, v2))
    {
    same_source:
      /* comparing equal sources */
      switch (py_op & ~ COMPARE_UNSIGNED) {
      case Py_LE:
      case Py_EQ:
      case Py_GE:
        return CC_ALWAYS_TRUE;
      default:
        return CC_ALWAYS_FALSE;
      }
    }
  if (is_compiletime(v1->source))
    if (is_compiletime(v2->source))
      {
        long a = CompileTime_Get(v1->source)->value;
        long b = CompileTime_Get(v2->source)->value;
        return immediate_compare(a, b, py_op);
      }
    else
      {
        vinfo_t* tmp;
        /* invert the two operands because the processor has only CMP xxx,immed
           and not CMP immed,xxx */
        result = inverted_results[py_op];
        tmp = v1;
        v1 = v2;
        v2 = tmp;
      }
  else
    {
      result = direct_results[py_op];
    }
  if (is_compiletime(v2->source))
    {
      result = int_cmp_i(po, v1, CompileTime_Get(v2->source)->value, result);
    }
  else
    {
      BEGIN_CODE
      NEED_CC();
      RTVINFO_IN_REG(v1);         /* CMP v1, v2 */
      COMMON_INSTR_FROM_RT(7, getreg(v1->source), v2->source);
      END_CODE
    }
  return result;
}

DEFINEFN
condition_code_t integer_cmp_i(PsycoObject* po, vinfo_t* v1,
                               long value2, int py_op)
{
  if (!compute_vinfo(v1, po)) return CC_ERROR;
  
  if (is_compiletime(v1->source))
    {
      long a = CompileTime_Get(v1->source)->value;
      return immediate_compare(a, value2, py_op);
    }
  else
    return int_cmp_i(po, v1, value2, direct_results[py_op]);
}

#if 0
DEFINEFN      (not used)
vinfo_t* integer_seqindex(PsycoObject* po, vinfo_t* vi, vinfo_t* vn, bool ovf)
{
  NonVirtualSource vns, vis;
  vns = vinfo_compute(vn, po);
  if (vns == SOURCE_ERROR) return NULL;
  vis = vinfo_compute(vi, po);
  if (vis == SOURCE_ERROR) return NULL;
  
  if (!is_compiletime(vis))
    {
      reg_t rg, tmprg;
      BEGIN_CODE
      NEED_CC_SRC(vis);
      NEED_FREE_REG(rg);
      LOAD_REG_FROM_RT(vis, rg);
      DELAY_USE_OF(rg);
      NEED_FREE_REG(tmprg);

      /* Increase 'rg' by 'vns' unless it is already in the range(0, vns). */
         /* CMP i, n */
      vns = vn->source;  /* reload, could have been moved by NEED_FREE_REG */
      COMMON_INSTR_FROM(7, rg, vns);
         /* SBB t, t */
      COMMON_INSTR_FROM_RT(3, tmprg, RunTime_New(tmprg, false...));
         /* AND t, n */
      COMMON_INSTR_FROM(4, tmprg, vns);
         /* SUB i, t */
      COMMON_INSTR_FROM_RT(5, rg, RunTime_New(tmprg, false...));
         /* ADD i, n */
      COMMON_INSTR_FROM(0, rg, vns);
      END_CODE

      if (ovf && runtime_condition_f(po, CC_NB))  /* if out of range */
        return NULL;
      return new_rtvinfo(po, rg, false...);
    }
  else
    {
      long index = CompileTime_Get(vis)->value;
      long reqlength;
      if (index >= 0)
        reqlength = index;  /* index is known, length must be greater than it */
      else
        reqlength = ~index;  /* idem for negative index */
      if (ovf)
        {
          /* test for out of range index -- more precisely, test that the
             length is not large enough for the known index */
          condition_code_t cc = integer_cmp_i(po, vn, reqlength, Py_LE);
          if (cc == CC_ERROR || runtime_condition_f(po, cc))
            return NULL;
        }
      if (index >= 0)
        {
          vinfo_incref(vi);
          return vi;
        }
      else
        return integer_add_i(po, vn, index...);
    }
}
#endif  /* 0 */

DEFINEFN
vinfo_t* integer_conditional(PsycoObject* po, condition_code_t cc,
                             long immed_true, long immed_false)
{
  reg_t rg;
  
  switch (cc) {
  case CC_ALWAYS_FALSE:
    return vinfo_new(CompileTime_New(immed_false));

  case CC_ALWAYS_TRUE:
    return vinfo_new(CompileTime_New(immed_true));

  default:
    BEGIN_CODE
    NEED_FREE_REG(rg);
    LOAD_REG_FROM_IMMED(rg, immed_true);
    SHORT_COND_JUMP_TO(code + SIZE_OF_SHORT_CONDITIONAL_JUMP
                            + SIZE_OF_LOAD_REG_FROM_IMMED, cc);
    LOAD_REG_FROM_IMMED(rg, immed_false);
    END_CODE
      
    return new_rtvinfo(po, rg, false, false);
  }
}

DEFINEFN
vinfo_t* make_runtime_copy(PsycoObject* po, vinfo_t* v)
{
	reg_t rg;
	if (!compute_vinfo(v, po)) return NULL;
	BEGIN_CODE
	NEED_FREE_REG(rg);
	LOAD_REG_FROM(v->source, rg);
	END_CODE
	return new_rtvinfo(po, rg, false, is_nonneg(v->source));
}