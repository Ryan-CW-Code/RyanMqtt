/*********************************************************************************************************
 *  ------------------------------------------------------------------------------------------------------
 *  file description
 *  ------------------------------------------------------------------------------------------------------
 *         \file  valloc.h
 *         \unit  valloc
 *        \brief  Test how much space is allocated
 *       \author  Lamdonn
 *      \details  v1.0.0
 ********************************************************************************************************/
#ifndef __valloc_H
#define __valloc_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

extern void *v_malloc(size_t size);
extern void *v_calloc(size_t num, size_t size);
extern void v_free(void *block);
extern void *v_realloc(void *block, size_t size);
extern int v_mcheck(int *dstCount, int *dstUse);
extern void displayMem(void);
extern void vallocInit(void);

#ifdef __cplusplus
}
#endif

#endif
