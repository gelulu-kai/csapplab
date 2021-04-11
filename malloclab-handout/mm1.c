/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//字大小和双字大小
#define WSIZE 4
#define DSIZE 8
//当堆内存不够时，向内核申请的堆空间4096
#define CHUNKSIZE (1<<12)
//将val放入p开始的4字节中
#define PUT(p,val) (*(unsigned int*)(p) = (val))
//获得头部和脚部的编码
#define PACK(size, alloc) ((size) | (alloc))
//从头部或脚部获得块大小和已分配位
#define GET_SIZE(p) (*(unsigned int*)(p) & ~0x7)
#define GET_ALLO(p) (*(unsigned int*)(p) & 0x1)
//获得块的头部和脚部
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
//获得上一个块和下一个块
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))//size of current block
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))//size of prev block

#define MAX(x,y) ((x)>(y)?(x):(y))

static char *heap_listp;

static void *extend_heap(size_t words);
static void *imme_coalesce(void *bp);
static void *first_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);//padding
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));//序言块
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));//结尾块

    heap_listp += DSIZE;

    if ((extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;
    
    return 0;
}

static void *extend_heap(size_t words)
{
    void *bp;
    size_t size;

    size = (words%2) ? (words+1)*WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));// New epilogue header

    return imme_coalesce(bp);
    // return bp;
}

static void *imme_coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLO(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLO(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else //if(!prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }

void *mm_malloc(size_t size)
{
    size_t asize, extend_size;
    char *bp;

    if (size == 0)
        return NULL;

    asize = size<=DSIZE ? 2*DSIZE : DSIZE * ((DSIZE + size + (DSIZE - 1)) / DSIZE);//at least 16 bytes
    // if((bp = first_fit(asize)) != NULL)
    // {
    //     place(bp, asize);
    //     return bp;
    // }
    if((bp = best_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extend_size = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *first_fit(size_t asize)
{
    void *bp = heap_listp;
    size_t size;

    while((size = GET_SIZE(HDRP(bp))) != 0)
    {
        if(size >= asize && !GET_ALLO(HDRP(bp)))
            return bp;
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

static void *best_fit(size_t asize)
{
    void *bp = heap_listp;
    size_t size, min_size = 0;
    void *best = NULL;//best pointer

    while((size = GET_SIZE(HDRP(bp))) != 0)
    {
        if(size >= asize && !GET_ALLO(HDRP(bp)) && (min_size == 0 || min_size>size))
        {
            best = bp;
            min_size = size;
        }
        bp = NEXT_BLKP(bp);
    }
    return best;
}

static void place(void *bp, size_t asize)
{
    size_t remain_size;

    remain_size = GET_SIZE(HDRP(bp)) - asize;
    if(remain_size > DSIZE)//resegment
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
    }
    else{
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    imme_coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;
    
//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//       return NULL;
//     copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     if (size < copySize)
//       copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize, ptr_size;
    void *new_bp;

    if (ptr == NULL)
        return mm_malloc(size);
    if(size == 0)
    {
        mm_free(ptr);
        return NULL;
    }

    asize = size<= DSIZE ? 2*DSIZE : DSIZE * ((DSIZE + size + (DSIZE - 1)) / DSIZE);//at least 16 bytes
    new_bp = imme_coalesce(ptr);//try if have free block
    ptr_size = GET_SIZE(HDRP(new_bp));
    PUT(HDRP(new_bp), PACK(ptr_size, 1));
    PUT(FTRP(new_bp), PACK(ptr_size, 1));
    if(new_bp != ptr)
        memcpy(new_bp, ptr, GET_SIZE(HDRP(ptr)) - DSIZE);
    
    
    if(ptr_size == asize)
        return new_bp;
    else if(ptr_size > asize)
    {
        place(new_bp, asize);
        return new_bp;
    }
    else
    {
        ptr = mm_malloc(asize);
        if(ptr == NULL)
            return NULL;
        
        memcpy(ptr, new_bp, GET_SIZE(HDRP(new_bp)) - DSIZE);
        mm_free(new_bp);
        return ptr;
    }
}













