 /***************************************************************/
/***          Automatically generated support file             ***/
 /***************************************************************/

 /* This file is automatically generated by 'files.py'.
    DO NOT MODIFY. Changes will be overwritten ! */


  /* internal part for psyco.c */
#if ALL_STATIC
# include "iprocessor.c"
# include "idispatcher.c"
# include "ipyencoding.c"
#else /* if !ALL_STATIC */
  EXTERNFN void psyco_processor_init(void);	/* iprocessor.c */
#endif /* !ALL_STATIC */

inline void initialize_processor_files(void) {
  psyco_processor_init();	/* iprocessor.c */
}