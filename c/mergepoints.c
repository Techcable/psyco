#include "mergepoints.h"
#include "vcompiler.h"
#include "stats.h"
#include "Python/pycinternal.h"

/* set to 1 to compute the detailed control flow
   which allows for early variable deletion */
#define FULL_CONTROL_FLOW    1
#define DUMP_CONTROL_FLOW    0


 /***************************************************************/
/***                Tables of code merge points                ***/
 /***************************************************************/

/* for each code object we build a bitarray specifying which
   positions in the byte code are potential merge points.
   A "merge point" is an instruction which can be executed
   immediately after two or more other instructions,
   typically jump targets.

   The respawn mecanisms (see psyco_prepare_respawn()) require
   that a merge point be also added after each instruction
   whose produced machine code might depend on external data.
   We should also avoid too long uninterrupted ranges of
   instructions without a single merge point.
*/

/* instructions that cause an unconditional jump: */
#define IS_JUMP_INSTR(op)    (op == BREAK_LOOP ||     \
                              op == RETURN_VALUE ||   \
                              op == JUMP_FORWARD ||   \
                              op == JUMP_ABSOLUTE ||  \
                              op == CONTINUE_LOOP ||  \
                              op == RAISE_VARARGS ||  \
                              IS_EPILOGUE_INSTR(op))

/* instructions with a target: */
#define HAS_JREL_INSTR(op)   (op == JUMP_FORWARD ||   \
                              op == JUMP_IF_FALSE ||  \
                              op == JUMP_IF_TRUE ||   \
                              op == FOR_OPCODE ||     \
                              /*    SETUP_LOOP replaced by FOR_OPCODE */    \
                              op == SETUP_EXCEPT ||   \
                              op == SETUP_FINALLY)

#define HAS_JABS_INSTR(op)   (op == JUMP_ABSOLUTE ||  \
                              op == CONTINUE_LOOP)

/* instructions whose target may be jumped to several times: */
#define HAS_J_MULTIPLE(op)   (op == FOR_OPCODE ||     \
                              /*    SETUP_LOOP replaced by FOR_OPCODE */    \
                              op == SETUP_EXCEPT ||   \
                              op == SETUP_FINALLY)

/* instructions producing code dependent on the context: */
#define IS_CTXDEP_INSTR(op)  (op == LOAD_GLOBAL)

#define MAX_UNINTERRUPTED_RANGE   300  /* bytes of Python bytecode */

/* opcodes that never produce any machine code or on which for some
   other reason there is no point in setting a merge point -- the
   merge point can simply be moved on to the next instruction */
#define NO_MERGE_POINT(op)  (IS_JUMP_INSTR(op) ||       \
                             HAS_JREL_INSTR(op) ||      \
                             HAS_JABS_INSTR(op) ||      \
                             IS_SET_LINENO(op) ||       \
                             op == POP_TOP ||           \
                             op == ROT_TWO ||           \
                             op == ROT_THREE ||         \
                             op == ROT_FOUR ||          \
                             op == DUP_TOP ||           \
                             op == DUP_TOPX ||          \
                             op == POP_BLOCK ||         \
                             op == END_FINALLY ||       \
                             op == LOAD_CONST ||        \
                             op == LOAD_FAST ||         \
                             op == STORE_FAST ||        \
                             op == DELETE_FAST)

/* all other supported instructions must be listed here. */
#define OTHER_OPCODE(op)  (op == UNARY_POSITIVE ||            \
                           op == UNARY_NEGATIVE ||            \
                           op == UNARY_NOT ||                 \
                           op == UNARY_CONVERT ||             \
                           op == UNARY_INVERT ||              \
                           op == BINARY_POWER ||              \
                           op == BINARY_MULTIPLY ||           \
                           op == BINARY_DIVIDE ||             \
                           op == BINARY_MODULO ||             \
                           op == BINARY_ADD ||                \
                           op == BINARY_SUBTRACT ||           \
                           op == BINARY_SUBSCR ||             \
                           op == BINARY_LSHIFT ||             \
                           op == BINARY_RSHIFT ||             \
                           op == BINARY_AND ||                \
                           op == BINARY_XOR ||                \
                           op == BINARY_OR ||                 \
                           IS_NEW_DIVIDE(op) ||               \
                           op == INPLACE_POWER ||             \
                           op == INPLACE_MULTIPLY ||          \
                           op == INPLACE_DIVIDE ||            \
                           op == INPLACE_MODULO ||            \
                           op == INPLACE_ADD ||               \
                           op == INPLACE_SUBTRACT ||          \
                           op == INPLACE_LSHIFT ||            \
                           op == INPLACE_RSHIFT ||            \
                           op == INPLACE_AND ||               \
                           op == INPLACE_XOR ||               \
                           op == INPLACE_OR ||                \
                           op == SLICE+0 ||                   \
                           op == SLICE+1 ||                   \
                           op == SLICE+2 ||                   \
                           op == SLICE+3 ||                   \
                           op == STORE_SLICE+0 ||             \
                           op == STORE_SLICE+1 ||             \
                           op == STORE_SLICE+2 ||             \
                           op == STORE_SLICE+3 ||             \
                           op == DELETE_SLICE+0 ||            \
                           op == DELETE_SLICE+1 ||            \
                           op == DELETE_SLICE+2 ||            \
                           op == DELETE_SLICE+3 ||            \
                           op == STORE_SUBSCR ||              \
                           op == DELETE_SUBSCR ||             \
                           op == PRINT_EXPR ||                \
                           op == PRINT_ITEM ||                \
                           op == PRINT_ITEM_TO ||             \
                           op == PRINT_NEWLINE ||             \
                           op == PRINT_NEWLINE_TO ||          \
                           op == BUILD_CLASS ||               \
                           op == UNPACK_SEQUENCE ||           \
                           op == STORE_ATTR ||                \
                           op == DELETE_ATTR ||               \
                           op == STORE_GLOBAL ||              \
                           op == DELETE_GLOBAL ||             \
                           op == LOAD_GLOBAL ||               \
                           op == BUILD_TUPLE ||               \
                           op == BUILD_LIST ||                \
                           op == BUILD_MAP ||                 \
                           op == LOAD_ATTR ||                 \
                           /* COMPARE_OP special-cased */     \
                           op == IMPORT_NAME ||               \
                           op == IMPORT_FROM ||               \
                           IS_GET_ITER(op) ||                 \
                           op == CALL_FUNCTION ||             \
                           op == CALL_FUNCTION_VAR ||         \
                           op == CALL_FUNCTION_KW ||          \
                           op == CALL_FUNCTION_VAR_KW ||      \
                           op == MAKE_FUNCTION ||             \
                           op == BUILD_SLICE ||               \
                           op == SETUP_LOOP)

#define SUPPORTED_COMPARE_ARG(oparg)  ( \
                              (oparg) == Py_LT ||  \
                              (oparg) == Py_LE ||  \
                              (oparg) == Py_EQ ||  \
                              (oparg) == Py_NE ||  \
                              (oparg) == Py_GT ||  \
                              (oparg) == Py_GE ||  \
                              (oparg) == PyCmp_IS        ||  \
                              (oparg) == PyCmp_IS_NOT    ||  \
                              (oparg) == PyCmp_IN        ||  \
                              (oparg) == PyCmp_NOT_IN    ||  \
                              (oparg) == PyCmp_EXC_MATCH ||  \
                              0)

 /***************************************************************/
#ifdef FOR_ITER
# define FOR_OPCODE              FOR_ITER
#else
# define FOR_OPCODE              FOR_LOOP
#endif

#ifdef GET_ITER
# define IS_GET_ITER(op)        (op == GET_ITER)
#else
# define IS_GET_ITER(op)         0
#endif

#ifdef RETURN_NONE
# define IS_EPILOGUE_INSTR(op)  (op == RETURN_NONE)
#else
# define IS_EPILOGUE_INSTR(op)   0
#endif

#ifdef SET_LINENO
# define IS_SET_LINENO(op)      (op == SET_LINENO)
#else
# define IS_SET_LINENO(op)       0
#endif

#ifdef BINARY_FLOOR_DIVIDE
# define IS_NEW_DIVIDE(op)      (op == BINARY_FLOOR_DIVIDE ||         \
                                 op == BINARY_TRUE_DIVIDE ||          \
                                 op == INPLACE_FLOOR_DIVIDE ||        \
                                 op == INPLACE_TRUE_DIVIDE)
#else
# define IS_NEW_DIVIDE(op)       0
#endif
 /***************************************************************/


#define MP_OTHER           0x01
#define MP_IS_JUMP         0x02
#define MP_HAS_JREL        0x04
#define MP_HAS_JABS        0x08
#define MP_HAS_J_MULTIPLE  0x10
#define MP_IS_CTXDEP       0x20
#define MP_NO_MERGEPT      0x40

#define F(op)      ((IS_JUMP_INSTR(op)    ? MP_IS_JUMP        : 0) |    \
                    (HAS_JREL_INSTR(op)   ? MP_HAS_JREL       : 0) |    \
                    (HAS_JABS_INSTR(op)   ? MP_HAS_JABS       : 0) |    \
                    (HAS_J_MULTIPLE(op)   ? MP_HAS_J_MULTIPLE : 0) |    \
                    (IS_CTXDEP_INSTR(op)  ? MP_IS_CTXDEP      : 0) |    \
                    (NO_MERGE_POINT(op)   ? MP_NO_MERGEPT     : 0) |    \
                    (OTHER_OPCODE(op)     ? MP_OTHER          : 0))

/* opcode table -- the preprocessor expands this into several hundreds KB
   of code (which reduces down to 256 bytes!).  Hopefully not a problem
   for modern C compilers */
static const char instr_control_flow[256] = {
  F(0x00), F(0x01), F(0x02), F(0x03), F(0x04), F(0x05), F(0x06), F(0x07),
  F(0x08), F(0x09), F(0x0A), F(0x0B), F(0x0C), F(0x0D), F(0x0E), F(0x0F),
  F(0x10), F(0x11), F(0x12), F(0x13), F(0x14), F(0x15), F(0x16), F(0x17),
  F(0x18), F(0x19), F(0x1A), F(0x1B), F(0x1C), F(0x1D), F(0x1E), F(0x1F),
  F(0x20), F(0x21), F(0x22), F(0x23), F(0x24), F(0x25), F(0x26), F(0x27),
  F(0x28), F(0x29), F(0x2A), F(0x2B), F(0x2C), F(0x2D), F(0x2E), F(0x2F),
  F(0x30), F(0x31), F(0x32), F(0x33), F(0x34), F(0x35), F(0x36), F(0x37),
  F(0x38), F(0x39), F(0x3A), F(0x3B), F(0x3C), F(0x3D), F(0x3E), F(0x3F),
  F(0x40), F(0x41), F(0x42), F(0x43), F(0x44), F(0x45), F(0x46), F(0x47),
  F(0x48), F(0x49), F(0x4A), F(0x4B), F(0x4C), F(0x4D), F(0x4E), F(0x4F),
  F(0x50), F(0x51), F(0x52), F(0x53), F(0x54), F(0x55), F(0x56), F(0x57),
  F(0x58), F(0x59), F(0x5A), F(0x5B), F(0x5C), F(0x5D), F(0x5E), F(0x5F),
  F(0x60), F(0x61), F(0x62), F(0x63), F(0x64), F(0x65), F(0x66), F(0x67),
  F(0x68), F(0x69), F(0x6A), F(0x6B), F(0x6C), F(0x6D), F(0x6E), F(0x6F),
  F(0x70), F(0x71), F(0x72), F(0x73), F(0x74), F(0x75), F(0x76), F(0x77),
  F(0x78), F(0x79), F(0x7A), F(0x7B), F(0x7C), F(0x7D), F(0x7E), F(0x7F),
  F(0x80), F(0x81), F(0x82), F(0x83), F(0x84), F(0x85), F(0x86), F(0x87),
  F(0x88), F(0x89), F(0x8A), F(0x8B), F(0x8C), F(0x8D), F(0x8E), F(0x8F),
  F(0x90), F(0x91), F(0x92), F(0x93), F(0x94), F(0x95), F(0x96), F(0x97),
  F(0x98), F(0x99), F(0x9A), F(0x9B), F(0x9C), F(0x9D), F(0x9E), F(0x9F),
  F(0xA0), F(0xA1), F(0xA2), F(0xA3), F(0xA4), F(0xA5), F(0xA6), F(0xA7),
  F(0xA8), F(0xA9), F(0xAA), F(0xAB), F(0xAC), F(0xAD), F(0xAE), F(0xAF),
  F(0xB0), F(0xB1), F(0xB2), F(0xB3), F(0xB4), F(0xB5), F(0xB6), F(0xB7),
  F(0xB8), F(0xB9), F(0xBA), F(0xBB), F(0xBC), F(0xBD), F(0xBE), F(0xBF),
  F(0xC0), F(0xC1), F(0xC2), F(0xC3), F(0xC4), F(0xC5), F(0xC6), F(0xC7),
  F(0xC8), F(0xC9), F(0xCA), F(0xCB), F(0xCC), F(0xCD), F(0xCE), F(0xCF),
  F(0xD0), F(0xD1), F(0xD2), F(0xD3), F(0xD4), F(0xD5), F(0xD6), F(0xD7),
  F(0xD8), F(0xD9), F(0xDA), F(0xDB), F(0xDC), F(0xDD), F(0xDE), F(0xDF),
  F(0xE0), F(0xE1), F(0xE2), F(0xE3), F(0xE4), F(0xE5), F(0xE6), F(0xE7),
  F(0xE8), F(0xE9), F(0xEA), F(0xEB), F(0xEC), F(0xED), F(0xEE), F(0xEF),
  F(0xF0), F(0xF1), F(0xF2), F(0xF3), F(0xF4), F(0xF5), F(0xF6), F(0xF7),
  F(0xF8), F(0xF9), F(0xFA), F(0xFB), F(0xFC), F(0xFD), F(0xFE), F(0xFF),
};
#undef F


struct instrnode_s {
  struct instrnode_s* next1;  /* next instruction */
  struct instrnode_s* next2;  /* next instr (jump target) */
  struct instrnode_s* next3;  /* next instr (exception handler) */
  unsigned char opcode;   /* copy of the instruction opcode */
  unsigned char back;     /* # of bytes to go back to find the instruction */
  unsigned char inpaths;  /* number of incoming paths to this point */
  unsigned char pending;  /* parse bytecode pending */
  global_entries_t* mp;   /* set a mergepoint here?  (see MPSET_XXX) */
  int mask;      /* mask of bits for needed variables */
  int storemask; /* mask of bits for stored variables */
};

#define MPSET_NEVER  ((global_entries_t*) NULL)
#define MPSET_MAYBE  ((global_entries_t*) -1)
#define MPSET_YES    ((global_entries_t*) -2)

inline int set_merge_point(struct instrnode_s* node)
{
  int runaway = 100;
  struct instrnode_s* bestchoice = node;
  while (node->next1 != NULL && (node->mp == MPSET_NEVER) && --runaway)
    {
      node = node->next1;
      if (node->inpaths >= 2)
        bestchoice = node;
    }
  if (node->mp == MPSET_YES)
    return 0;        /* found an already-set merge point */
  else
    {
      extra_assert(bestchoice->mp != MPSET_YES);
      bestchoice->mp = MPSET_YES;  /* set merge point */
      return 1;
    }
}


/***************************************************************/
#if FULL_CONTROL_FLOW

/* how many variables fit in the 'int' bitfield of instrnode_s */
#define VARS_PER_PASS   (sizeof(int)*8-1)

inline bool back_propagate_mask(struct instrnode_s* instrnodes,
                                struct instrnode_s* node,
                                int var0)
{
  bool modif = false;
  int prevmask, mask, oparg;
  while (node > instrnodes)
    {
      node--;
      oparg = node->mask;
      node -= node->back;  /* skip back argument */
      if (node->next1 != NULL)
        {
          prevmask = mask = node->mask;
          mask |= node->next1->mask;
          if (node->next2 != NULL)
            {
              mask |= node->next2->mask;
              if (node->next3 != NULL)
                  mask |= node->next3->mask;
            }
          if (node->opcode == STORE_FAST)
            {
              int bit = oparg - var0;
              if (0 <= bit && bit < VARS_PER_PASS)
                mask &= ~(1<<bit);
            }
          if (mask != prevmask)
            {
              node->mask = mask;
              modif = true;
            }
        }
      
    }
  return modif;
}

inline void mark_var_uses(struct instrnode_s* instrnodes,
                          struct instrnode_s* node,
                          int var0)
{
  while (node > instrnodes)
    {
      int m1 = 1<<VARS_PER_PASS;
      node--;
      if (node->back)
        {
          int oparg = node->mask;
          node -= node->back;  /* skip back argument */
          if (node->opcode == LOAD_FAST || node->opcode == DELETE_FAST)
            {
              int bit = oparg - var0;
              if (0 <= bit && bit < VARS_PER_PASS)
                m1 |= (1<<bit);
            }
        }
      node->mask = m1;
      node->storemask = 0;
    }
}

inline int function_args_mask(int var0, int ninitialized)
{
  int bits = ninitialized - var0;
  if (bits <= 0)
    return 0;
  else if (bits >= VARS_PER_PASS)
    return -1;
  else
    return (1<<bits)-1;
}

static void forward_propagate(struct instrnode_s* node, int newmask, int var0)
{
  while ((newmask |= node->storemask) != node->storemask)
    {
      node->storemask = newmask;
      if (!node->next1)
        break;
      if (node->mp)
        {
          /* at each mergepoint, we only keep variables that are
             also present in mode->mask. */
          newmask &= node->mask;
        }
      if (node->opcode == STORE_FAST)
        {
          /* after each STORE_FAST, add the corresponding bit into newmask */
          int oparg = node[1].mask;
          int bit = oparg - var0;
          if (0 <= bit && bit < VARS_PER_PASS)
            newmask |= (1<<bit);
        }
      if (node->next2)
        {
          forward_propagate(node->next2, newmask, var0);
          if (node->next3)
            forward_propagate(node->next3, newmask, var0);
        }
      node = node->next1;
    }
}

inline void find_unused_vars(struct instrnode_s* instrnodes,
                             struct instrnode_s* node,
                             int var0)
{
  while (node > instrnodes)
    {
      node--;
      node -= node->back;  /* skip back argument */
      if (node->mp)
        {
          /* if at that mergepoint we have no use for a variable,
             write a note to say it can be deleted.  */
          int remove = node->storemask & ~node->mask;
          int i;
          psyco_assert(node->mask & (1<<VARS_PER_PASS));
          for (i=var0; remove; i++, remove>>=1)
            if (remove & 1)
              psyco_ge_unused_var(node->mp, i);
        }
    }
}

static void analyse_variables(struct instrnode_s* instrnodes,
                              struct instrnode_s* end,
                              PyCodeObject* co)
{
  int var0;
  int nlocals = co->co_nlocals;
  int ninitialized = co->co_argcount;
  if (co->co_flags & CO_VARKEYWORDS) ninitialized++;
  if (co->co_flags & CO_VARARGS)     ninitialized++;
  
  for (var0 = 0; var0 < nlocals; var0 += VARS_PER_PASS)
    {
      mark_var_uses(instrnodes, end, var0);
      while (back_propagate_mask(instrnodes, end, var0))
        ;
      forward_propagate(instrnodes, (function_args_mask(var0, ninitialized) |
                                     (1<<VARS_PER_PASS)), var0);
      /*propagate_stores(instrnodes, end);*/
      find_unused_vars(instrnodes, end, var0);
    }

#if DUMP_CONTROL_FLOW
  /* debugging dump */
  {
    int i;
    fprintf(stderr, "mergepoints.c: %s:\n", PyCodeObject_NAME(co));
    for (i=0; instrnodes<end; i++,instrnodes++)
      if (instrnodes->mp)
        {
          PyObject* plist = instrnodes->mp->fatlist;
          int j;
          fprintf(stderr, "  line %d:   %d ", PyCode_Addr2Line(co, i), i);
          for (j=0; j<PyList_GET_SIZE(plist); j++)
            {
              int num;
              PyObject* o1 = PyList_GET_ITEM(plist, j);
              if (!PyInt_Check(o1))
                break;
              num = PyInt_AS_LONG(o1);
              o1 = PyTuple_GetItem(co->co_varnames, num);
              fprintf(stderr, " [%s]", PyString_AsString(o1));
            }
          fprintf(stderr, "\n");
        }
  }
#endif
}

#endif  /* FULL_CONTROL_FLOW */
/***************************************************************/


DEFINEFN
PyObject* psyco_build_merge_points(PyCodeObject* co)
{
  PyObject* s;
  mergepoint_t* mp;
  int mp_flags = MP_FLAGS_EXTRA;
  int length = PyString_GET_SIZE(co->co_code);
  unsigned char* source = (unsigned char*) PyString_AS_STRING(co->co_code);
  size_t ibytes = (length+1) * sizeof(struct instrnode_s);
  struct instrnode_s* instrnodes;
  int i, lasti, count;
  bool modif;
  PyTryBlock blockstack[CO_MAXBLOCKS];
  int iblock;
  int valid_controlflow = 1;

  if (length == 0)
    {
      /* normally a code object's code string is never empty,
         but pyexpat.c has some hacks that we have to work around */
      Py_INCREF(Py_None);
      return Py_None;
    }
  instrnodes = (struct instrnode_s*) PyMem_MALLOC(ibytes);
  if (instrnodes == NULL)
    OUT_OF_MEMORY();
  memset(instrnodes, 0, ibytes);

  /* parse the bytecode once, filling the instrnodes[].opcode,back,mask fields */
  iblock = 0;
  for (i=0; i<length; )
    {
      int oparg = 0;
      int i0 = i;
      int btop;
      unsigned char op = source[i++];
      instrnodes[i0].opcode = op;
      if (HAS_ARG(op))
        {
          i += 2;
          oparg = (source[i-1]<<8) + source[i-2];
          if (op == EXTENDED_ARG)
            {
              op = source[i++];
              psyco_assert(HAS_ARG(op) && op != EXTENDED_ARG);
              i += 2;
              oparg = oparg<<16 | ((source[i-1]<<8) + source[i-2]);
            }
          instrnodes[i0+1].back = instrnodes[i-1].back = (i-1) - i0;
          instrnodes[i0+1].mask = instrnodes[i-1].mask = oparg;  /* save oparg */
        }
      for (btop = iblock; btop--; )
        {
          if (blockstack[btop].b_type == SETUP_EXCEPT ||
              blockstack[btop].b_type == SETUP_FINALLY) {
            /* control flow may jump to the b_handler at any time */
            instrnodes[i0].next3 = instrnodes + blockstack[btop].b_handler;
            break;
          }
        }
      switch (op)
        {
        case SETUP_EXCEPT:
        case SETUP_FINALLY:
        case SETUP_LOOP:
          switch (op) {
          case SETUP_EXCEPT:  mp_flags |= MP_FLAGS_HAS_EXCEPT;  break;
          case SETUP_FINALLY: mp_flags |= MP_FLAGS_HAS_FINALLY; break;
          }
          psyco_assert(iblock < CO_MAXBLOCKS);
          blockstack[iblock].b_type = op;
          blockstack[iblock].b_handler = i + oparg;
          iblock++;
          break;

        case POP_BLOCK:
          psyco_assert(iblock > 0);
          iblock--;
          break;

        case BREAK_LOOP:
          /* break the innermost loop */
          btop = iblock;
          do {
            psyco_assert(btop>0);
            btop--;
          } while (blockstack[btop].b_type != SETUP_LOOP &&
                   blockstack[btop].b_type != SETUP_FINALLY);
          /* jump to the loop bottom or finally handler */
          instrnodes[i0].next3 = instrnodes + blockstack[btop].b_handler;
          if (blockstack[btop].b_type != SETUP_LOOP)
            {  /* argh, this gets messy */
               /* because END_FINALLY will then jump to the loop bottom */
              valid_controlflow = 0;
            }
          break;

        case CONTINUE_LOOP:
          btop = iblock;
          do {
            psyco_assert(btop>0);
            btop--;
          } while (blockstack[btop].b_type != SETUP_LOOP &&
                   blockstack[btop].b_type != SETUP_FINALLY);
          if (blockstack[btop].b_type == SETUP_LOOP)
            {  /* jump to the loop head */
              instrnodes[i0].next3 = instrnodes + oparg;
            }
          else
            {  /* argh, this gets messy */
              instrnodes[i0].next3 = instrnodes + blockstack[btop].b_handler;
              valid_controlflow = 0;
            }
          break;
        }
    }
  if (iblock != 0)
    valid_controlflow = 0;  /* ?? */

  /* control flow analysis */
  instrnodes[0].pending = 1;
  do
    {
      modif = false;
      for (i=0; i<length; i++)
        if (instrnodes[i].pending == 1)
          {
            unsigned char op = instrnodes[i].opcode;
            int oparg = instrnodes[i+1].mask;
            int nextinstr = i+1 + instrnodes[i+1].back;
            char flags = instr_control_flow[(int) op];
            if (flags == 0)
              if (op != COMPARE_OP || !SUPPORTED_COMPARE_ARG(oparg))
                {
                  /* unsupported instruction */
                  debug_printf(1 + (strcmp(PyCodeObject_NAME(co), "?")==0),
                               ("unsupported opcode %d at %s:%d\n",
                                (int) op, PyCodeObject_NAME(co), i));
                  PyMem_FREE(instrnodes);
                  Py_INCREF(Py_None);
                  return Py_None;
                }
            if (flags & (MP_HAS_JREL|MP_HAS_JABS))
              {
                int jtarget = oparg;
                if (flags & MP_HAS_JREL)
                  jtarget += nextinstr;
                instrnodes[jtarget].pending |= 1;
                if (flags & MP_HAS_J_MULTIPLE || !++instrnodes[jtarget].inpaths)
                  instrnodes[jtarget].inpaths = 99;
                instrnodes[i].next2 = instrnodes + jtarget;
              }
            
            if (!(flags & MP_IS_JUMP))
              {
                instrnodes[nextinstr].pending |= 1;
                if (flags & MP_IS_CTXDEP || !++instrnodes[nextinstr].inpaths)
                  instrnodes[nextinstr].inpaths = 99;
                instrnodes[i].next1 = instrnodes + nextinstr;
              }
            if (!(flags & MP_NO_MERGEPT))
              instrnodes[i].mp = MPSET_MAYBE;
            
            /* compact the next1-next2-next3 */
            if (instrnodes[i].next2 == NULL)
              {
                instrnodes[i].next2 = instrnodes[i].next3;
                instrnodes[i].next3 = NULL;
              }
            if (instrnodes[i].next1 == NULL)
              {
                instrnodes[i].next1 = instrnodes[i].next2;
                instrnodes[i].next2 = instrnodes[i].next3;
                instrnodes[i].next3 = NULL;
              }
            instrnodes[i].pending = 3;
            modif = true;
          }
    } while (modif);

  /* set and count merge points */
  instrnodes[0].mp = MPSET_YES;    /* there is a merge point at the beginning */
  count = 1;
  lasti = 0;
  for (i=1; i<length; i++)
    if (instrnodes[i].inpaths > 0)
      {
        /* set merge points when there are more than one incoming path */
        /* ensure there are merge points at regular intervals */
        if (instrnodes[i].inpaths >= 2 || i-lasti > MAX_UNINTERRUPTED_RANGE)
          {
            count += set_merge_point(instrnodes + i);
            lasti = i;
          }
      }
  
  /* allocate the string buffer, one mergepoint_t per merge point plus
     the room for a final negative bitfield flags. */
  ibytes = count * sizeof(mergepoint_t) + sizeof(int);
  s = PyString_FromStringAndSize(NULL, ibytes);
  if (s == NULL)
    OUT_OF_MEMORY();
  mp = (mergepoint_t*) PyString_AS_STRING(s);

  for (i=0; i<length; i++)
    {
      if (instrnodes[i].mp != MPSET_YES)
        instrnodes[i].mp = NULL;
      else
        {
          mp->bytecode_position = i;
          instrnodes[i].mp = &mp->entries;
          psyco_ge_init(&mp->entries);
          mp++;
        }
    }
  extra_assert(mp - (mergepoint_t*) PyString_AS_STRING(s) == count);
  mp->bytecode_position = mp_flags;
#if FULL_CONTROL_FLOW
  if (valid_controlflow)
    analyse_variables(instrnodes, instrnodes+length, co);
#endif
  PyMem_FREE(instrnodes);
  return s;
}

DEFINEFN
mergepoint_t* psyco_next_merge_point(PyObject* mergepoints,
                                     int position)
{
  mergepoint_t* array;
  int bufsize;
  extra_assert(PyString_Check(mergepoints));
  array = (mergepoint_t*) PyString_AS_STRING(mergepoints);
  bufsize = PyString_GET_SIZE(mergepoints);
  extra_assert((bufsize % sizeof(mergepoint_t)) == sizeof(int));
  bufsize /= sizeof(mergepoint_t);
  extra_assert(bufsize > 0);
  do {
    int test = bufsize/2;
    if (position > array[test].bytecode_position)
      {
        ++test;
        array += test;
        bufsize -= test;
      }
    else
      {
        bufsize = test;
      }
  } while (bufsize > 0);
  return array;
}

DEFINEFN
PyObject* psyco_get_merge_points(PyCodeObject* co)
{
  return PyCodeStats_MergePoints(PyCodeStats_Get(co));
}
