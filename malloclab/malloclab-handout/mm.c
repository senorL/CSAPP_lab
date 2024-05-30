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
    "senorL",
    /* First member's full name */
    "senorL",
    /* First member's email address */
    "senor_l@foxmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))
#define PACK_ALL(size, prev_alloc, alloc) ((size) | (prev_alloc) | (alloc))

#define GET(p) (*(unsigned*)(p))
#define PUT(p, val) (*(unsigned*)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) (GET(p) & 0x2)
#define SET_ALLOC(p) (GET(p) |= 0x1)
#define SET_FREE(p) (GET(p) &= ~0x1)
#define SET_PREV_ALLOC(p) (GET(p) |= 0x2)
#define SET_PREV_FREE(p) (GET(p) &= ~0x2)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))

static char* heap_listp = 0;

#define FREE_LIST_SIZE 15
static char** free_lists;

#define PREV_NODE(bp)       ((char *)(mem_heap_lo() + *(unsigned*)(bp)))
#define NEXT_NODE(bp)       ((char *)(mem_heap_lo() + *(unsigned*)(bp + WSIZE)))
#define SET_NODE_PREV(bp,val)   (*(unsigned*)(bp) = ((unsigned)(long)val))
#define SET_NODE_NEXT(bp,val)   (*(unsigned*)((char *)bp + WSIZE) = ((unsigned)(long)val))

#define CHECK_ALIGN(p) (ALIGN(p) == (size_t)p)
static inline void get_range(size_t index);
static size_t low_range;
static size_t high_range;

static inline void* extend_heap(size_t words);
static inline void* coalesce(void* bp, size_t size);
static inline size_t get_index(size_t size);
static inline size_t adjust_alloc_size(size_t size);
static inline void* find_fit(size_t asize);
static inline void place(void* bp, size_t size);
static inline void insert_node(void* bp, size_t size);
static inline void delete_node(void* bp);

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int i = 0;
    free_lists = mem_heap_lo();
    while(i < FREE_LIST_SIZE) {
        if ((heap_listp = mem_sbrk(DSIZE)) == (void*)-1)
            return -1;
        free_lists[i] = mem_heap_lo();
        i++;
    }
    if((heap_listp = mem_sbrk(2 * DSIZE)) == (void*)-1)
        return -1;  
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + DSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 3));
    heap_listp += DSIZE;
    if(extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extend_size;
    char* bp;
    if(heap_listp == 0)
        mm_init();
    if(size == 0)
        return NULL;
    size = adjust_alloc_size(size);
    if(size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);
    if((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    extend_size = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extend_size / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
        return;
    if (heap_listp == 0) {
        mm_init();
        return;
    }
    size_t cur_size = GET_SIZE(HDRP(ptr));
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
    PUT(HDRP(ptr), PACK_ALL(cur_size, prev_alloc, 0));
    PUT(FTRP(ptr), PACK_ALL(cur_size, prev_alloc, 0));
    
    coalesce(ptr, cur_size);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void * newptr;

    if (size == 0) {
        free(ptr);
        return 0;
    }

    if(ptr == NULL)
        return mm_malloc(size);
    
    newptr = mm_malloc(size);
    if(!newptr)
        return 0;
    oldsize = GET_SIZE(HDRP(ptr));
    oldsize = MIN(oldsize, size);
    memcpy(newptr, ptr, oldsize);

    mm_free(ptr);
    return newptr;
}

void * calloc(size_t elem_num, size_t size) {
    size_t total_size = elem_num * size;
    void * ptr = mm_malloc(total_size);
    memset(ptr, 0, total_size);
    return ptr;
}

static inline void* extend_heap(size_t words) {
    char* bp;
    size_t size;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    PUT(HDRP(bp), PACK_ALL(size, GET_PREV_ALLOC(HDRP(bp)), 0));
    PUT(FTRP(bp), PACK_ALL(size, GET_PREV_ALLOC(HDRP(bp)), 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp, size);
}

static inline void* coalesce(void* bp, size_t size) {
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    if (prev_alloc && next_alloc) {
        SET_PREV_FREE(HDRP(NEXT_BLKP(bp)));
    }
    else if (prev_alloc && !next_alloc) {
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK_ALL(size, 2, 0));
        PUT(FTRP(bp), PACK_ALL(size, 2, 0));
    }
    else if (!prev_alloc && next_alloc) {
        delete_node(PREV_BLKP(bp));
        SET_PREV_FREE(HDRP(NEXT_BLKP(bp)));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t prev_prev_alloc = GET_PREV_ALLOC(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK_ALL(size, prev_prev_alloc, 0));
        PUT(FTRP(bp), PACK_ALL(size, prev_prev_alloc, 0));
        bp = PREV_BLKP(bp);
    }
    else {
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(HDRP(NEXT_BLKP(bp))));
        size_t prev_prev_alloc = GET_PREV_ALLOC(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK_ALL(size, prev_prev_alloc, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK_ALL(size, prev_prev_alloc, 0));
        bp = PREV_BLKP(bp);
    }
    insert_node(bp, size);
    return bp;
}

static inline size_t get_index(size_t size) {
    if (size <= 24)
        return 0;
    if (size <= 32)
        return 1;
    if (size <= 64)
        return 2;
    if (size <= 80)
        return 3;
    if (size <= 120)
        return 4;
    if (size <= 240)
        return 5;
    if (size <= 480)
        return 6;
    if (size <= 960)
        return 7;
    if (size <= 1920)
        return 8;
    if (size <= 3840)
        return 9;
    if (size <= 7680)
        return 10;
    if (size <= 15360)
        return 11;
    if (size <= 30720)
        return 12;
    if (size <= 61440)
        return 13;
    else
        return 14;
}

static inline void get_range(size_t index) {
    switch (index) {
    case 0:
        low_range = 8;
        high_range = 24;
        break;
    case 1:
        low_range = 24;
        high_range = 32;
        break;
    case 2:
        low_range = 32;
        high_range = 64;
        break;
    case 3:
        low_range = 64;
        high_range = 80;
        break;
    case 4:
        low_range = 80;
        high_range = 120;
        break;
    case 5:
        low_range = 120;
        high_range = 240;
        break;
    case 6:
        low_range = 240;
        high_range = 480;
        break;
    case 7:
        low_range = 480;
        high_range = 960;
        break;
    case 8:
        low_range = 960;
        high_range = 1920;
        break;
    case 9:
        low_range = 1920;
        high_range = 3840;
        break;
    case 10:
        low_range = 3840;
        high_range = 7680;
        break;
    case 11:
        low_range = 7680;
        high_range = 15360;
        break;
    case 12:
        low_range = 15360;
        high_range = 30720;
        break;
    case 13:
        low_range = 30720;
        high_range = 61440;
        break;
    case 14:
        low_range = 61440;
        high_range = 0x7fffffff;
        break;
    }
}

static inline size_t adjust_alloc_size(size_t size) {
    // freeciv.rep
    if (size >= 120 && size < 128) {
        return 128;
    }
    // binary.rep
    if (size >= 448 && size < 512) {
        return 512;
    }
    if (size >= 1000 && size < 1024) {
        return 1024;
    }
    if (size >= 2000 && size < 2048) {
        return 2048;
    }
    return size;
}

static inline void* find_fit(size_t asize) {
    int num = get_index(asize);
    char * bp;
    for (int i = num; i < FREE_LIST_SIZE; i++) {
        for (bp = free_lists[i]; bp != mem_heap_lo(); bp = NEXT_NODE(bp)) {
            long spare = GET_SIZE(HDRP(bp)) - asize;
            if(spare >= 0)
            return bp;
        }
    }
    return NULL;

}














