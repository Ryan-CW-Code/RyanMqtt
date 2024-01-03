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
extern "C"
{
#endif

#include <stdlib.h>

    void *v_malloc(size_t size);
    void *v_calloc(size_t num, size_t size);
    void v_free(void *block);
    void *v_realloc(void *block, size_t size);
    int v_mcheck(int *_count, int *_use);
    void displayMem();
    void vallocInit();

#define malloc v_malloc
#define calloc v_calloc
#define free v_free
#define realloc v_realloc

#ifdef __cplusplus
}
#endif

#endif
