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
//当堆内存不够时，向内核申请的堆空间
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
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

//获得块中记录后继和前驱的地址
#define PRED(bp) ((char*)(bp) + WSIZE)
#define SUCC(bp) ((char*)bp)
//获得块的后继和前驱的地址
#define PRED_BLKP(bp) (*(unsigned int*)(PRED(bp)))
#define SUCC_BLKP(bp) (*(unsigned int*)(SUCC(bp)))

#define MAX(x,y) ((x)>(y)?(x):(y))
// {16-31},{32-63},{64-127},{128-255},{256-511},{512-1023},{1024-2047},{2048-4095},{4096-inf}

static char *heap_listp;
static char *listp;

static void *extend_heap(size_t words);
static void *imme_coalesce(void *bp);
static void *first_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *add_block(void *bp);
static int Index(size_t size);
static void *LIFO(void *bp, void *root);
static void *AddressOrder(void *bp, void *root);
static void print_listp();

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(12*WSIZE)) == (void *)-1)
        return -1;
    //空闲块的最小块包含头部、前驱、后继和脚部，有16字节
    PUT(heap_listp + 0*WSIZE, NULL);
    PUT(heap_listp + 1*WSIZE, NULL);
    PUT(heap_listp + 2*WSIZE, NULL);
    PUT(heap_listp + 3*WSIZE, NULL);
    PUT(heap_listp + 4*WSIZE, NULL);
    PUT(heap_listp + 5*WSIZE, NULL);
    PUT(heap_listp + 6*WSIZE, NULL);
    PUT(heap_listp + 7*WSIZE, NULL);
    PUT(heap_listp + 8*WSIZE, NULL);//root point

    PUT(heap_listp + 9*WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 10*WSIZE, PACK(DSIZE, 1));//序言块 8byte
    PUT(heap_listp + 11*WSIZE, PACK(0, 1));//结尾块

    listp = heap_listp;
    heap_listp += 10*WSIZE;

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

    PUT(PRED(bp), NULL);
    PUT(SUCC(bp), NULL);//set pointer NULL

    bp =  imme_coalesce(bp);
    bp = add_block(bp);//加入链表
    return bp;
}

static void *add_block(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int index = Index(size);
    void *root = listp + index * WSIZE;//确定大小类
    return LIFO(bp, root);
    // return AddressOrder(bp, root);
}

static int Index(size_t size){
    int ind = 0;
    if(size >= 4096)
        return 8;

    size = size>>5;
    while(size){
        size = size>>1;
        ind++;
    }
    return ind;
}

static void *LIFO(void *bp, void *root)//直接将bp插入root后面later in first out
{
    PUT(SUCC(root), bp);
    PUT(PRED(bp), root);
    if(SUCC_BLKP(root) != NULL){
        PUT(PRED(SUCC_BLKP(root)), bp);//succ->bp
        PUT(SUCC(bp), SUCC_BLKP(root));//bp->succ
    }else{
        PUT(SUCC(bp), NULL);
    }
    return bp;
}
static void *AddressOrder(void *bp, void *root)//按照显示列表地址递增顺序插入
{
    void *succ = root;
    while(SUCC_BLKP(succ) != NULL){
        succ = SUCC_BLKP(succ);
        if(succ >= bp)
            break;        
    }
    if(succ == root) return LIFO(bp, root);
    else if(SUCC_BLKP(succ) == NULL)//bp>=succ
    {
        PUT(SUCC(succ), bp);
        PUT(SUCC(bp), NULL);
        PUT(PRED(bp), succ);
    }
    else{ //bp->succc
        PUT(SUCC(PRED_BLKP(succ)), bp);
        PUT(PRED(succ), bp);
        PUT(SUCC(bp), succ);
        PUT(PRED(bp), PRED_BLKP(succ));
    }
    return bp;
}       

static void *delet_block(void *bp){
    PUT(SUCC(PRED_BLKP(bp)), SUCC_BLKP(bp));
    if ((SUCC_BLKP(bp)) != NULL)//bp 有后继点
        PUT(PRED(SUCC_BLKP(bp)), PRED_BLKP(bp));
}

static void *imme_coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLO(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLO(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delet_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        delet_block(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else //if(!prev_alloc && !next_alloc)
    {
        size += GET_SIZE(FTRP(PREV_BLKP(bp))) + 
            GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        delet_block(PREV_BLKP(bp));
        delet_block(NEXT_BLKP(bp));
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

    asize = size<=DSIZE ? 2*DSIZE : DSIZE * ((DSIZE + size + (DSIZE - 1)) / DSIZE);//at least 16 bytes 4words
    if((bp = first_fit(asize)) != NULL)
    {
        // print_listp();
        place(bp, asize);
        return bp;
    }
    // if((bp = best_fit(asize)) != NULL)
    // {
    //     place(bp, asize);
    //     return bp;
    // }

    extend_size = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    // print_listp();
    place(bp, asize);
    return bp;
}

static void *first_fit(size_t asize)
{
    int ind = Index(asize);
    void *succ;
    while(ind <= 8)
    {
        succ = listp + ind*WSIZE;
        while((succ = SUCC_BLKP(succ)) != NULL)
        {
            // succ = SUCC_BLKP(succ);
            if(GET_SIZE(HDRP(succ)) >= asize && !GET_ALLO(HDRP(succ)))
                return succ;
        }
        ind++;
    }
    return NULL;
}

static void *best_fit(size_t asize)
{
    int ind = Index(asize);
    void *succ;
    size_t size, min_size = 0;
    void *best = NULL;//best pointer

    while(ind <= 8)
    {
        succ = listp + ind*WSIZE;
        while((succ = SUCC_BLKP(succ)) != NULL)
        {
            size = GET_SIZE(HDRP(succ));
            if(size >= asize && !GET_ALLO(HDRP(succ)) && (min_size == 0 || min_size>size)){
                best = succ;
                min_size = size;
            }
            // succ = SUCC_BLKP(succ);
        }
        if(best != NULL)
            return best;
        ind+=1;
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t remain_size;

    remain_size = GET_SIZE(HDRP(bp)) - asize;
    delet_block(bp);
    if(remain_size >= 4*WSIZE)//resegment
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remain_size, 0));
        add_block(NEXT_BLKP(bp));
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


    ptr = imme_coalesce(ptr);
    add_block(ptr);
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
    size_t asize, ptr_size, remain_size;
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
    // add_block(new_bp);
    ptr_size = GET_SIZE(HDRP(new_bp));
    PUT(HDRP(new_bp), PACK(ptr_size, 1));
    PUT(FTRP(new_bp), PACK(ptr_size, 1));

    if(new_bp != ptr)
        memcpy(new_bp, ptr, GET_SIZE(HDRP(ptr)) - DSIZE);
    
    if(ptr_size == asize){
         return new_bp;
    }
    else if(ptr_size > asize)
    {
        // place(new_bp, asize)
        remain_size = ptr_size - asize;
        if(remain_size >= 4*WSIZE)//resegment
        {
            PUT(HDRP(new_bp), PACK(asize, 1));
            PUT(FTRP(new_bp), PACK(asize, 1));
            PUT(HDRP(NEXT_BLKP(new_bp)), PACK(remain_size, 0));
            PUT(FTRP(NEXT_BLKP(new_bp)), PACK(remain_size, 0));
            add_block(NEXT_BLKP(new_bp));
        } 
        return new_bp;
    }else{
        ptr = mm_malloc(asize);//first_fit
        if(ptr == NULL)
            return NULL;
        memcpy(ptr, new_bp, ptr_size - DSIZE);
        mm_free(new_bp);
        return ptr;
    }
}

static void print_listp(){
	int ind;
	void *node, *root;
	printf("print listp\n");
	for(ind=1;ind<=8;ind++){
		node = listp+ind*WSIZE;
		root = listp+ind*WSIZE;
		printf("%d:\n",ind);
		while(SUCC_BLKP(node)){
			node = SUCC_BLKP(node);
			printf("-->%p,%d",node, GET_SIZE(HDRP(node)));
		}
		printf("-->%p\n",SUCC_BLKP(node));
		while(node!=root){
			printf("<--%p,%d",node, GET_SIZE(HDRP(node)));
			node = PRED_BLKP(node);
		}
		printf("<--%p\n",node);
	}
}













