 /***************************************************************/
/***            Psyco equivalent of funcobject.h               ***/
 /***************************************************************/

#ifndef _PSY_FUNCOBJECT_H
#define _PSY_FUNCOBJECT_H


#include "pobject.h"
#include "pabstract.h"


/***************************************************************/
/* virtual functions.                                          */
/* 'fdefaults' may be NULL.                                    */
EXTERNFN vinfo_t* PsycoFunction_New(PsycoObject* po, vinfo_t* fcode,
                                    vinfo_t* fglobals, vinfo_t* fdefaults);


#endif /* _PSY_FUNCOBJECT_H */
