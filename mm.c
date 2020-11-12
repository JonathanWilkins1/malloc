/*
  Filename   : mm.c
  Author     : Jonathan Wilkins & Nick Loser
  Course     : CSCI 380-01
  Assignment : Assignment 8
  Description: Implements allocation
*/

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/****************************************************************/
// Useful type aliases

typedef uint64_t word;
typedef uint32_t tag;
typedef uint8_t byte;
typedef byte* address;

/****************************************************************/
// Globals

address g_heapBase;

/****************************************************************/
// Useful constants

const uint8_t WORD_SIZE = sizeof (word);
const uint8_t TAG_SIZE = sizeof (tag);
const uint8_t DWORD_SIZE = WORD_SIZE * 2;
const uint8_t OVERHEAD_BYTES = TAG_SIZE * 2;
// Add others...

/****************************************************************/
// Inline functions

static inline tag*
header (address p);

static inline bool
isAllocated (address p);

static inline uint32_t
sizeOf (address p);

static inline tag*
footer (address p);

static inline address
nextBlock (address p);

static inline tag*
prevFooter (address p);

static inline tag*
nextHeader (address p);

static inline address
prevBlock (address p);

static inline void
makeBlock (address p, uint32_t t, bool b);

static inline void
toggleBlock (address p);

int
mm_check (void);

static inline address
coalesce (address ptr);

void
printBlock (address p);

void
printPtrDiff (const char* header, void* p, void* base);

/****************************************************************/
// Non-inline functions

int
mm_init (void)
{
  if ((g_heapBase = mem_sbrk (8 * WORD_SIZE)) == (void*)-1)
  {
    return -1;
  }
  g_heapBase += DWORD_SIZE;
  *(g_heapBase - WORD_SIZE) = 0 | 1;
  *((address)mem_heap_hi () - TAG_SIZE) = 0 | 1;
  *header (g_heapBase) = 6 | 0;
  *((address)mem_heap_hi () - WORD_SIZE) = 6 | 0;
  mm_check();
  return 0;
}

/****************************************************************/

void*
mm_malloc (uint32_t size)
{
  fprintf (stderr, "allocate block of size %u\n", size);
  return NULL;
}

/****************************************************************/

void
mm_free (void* ptr)
{
  fprintf (stderr, "free block at %p\n", ptr);
  if (ptr == 0 || !isAllocated (ptr) || g_heapBase == 0)
  {
    return;
  }

  toggleBlock (ptr);
  coalesce (ptr);
}

/****************************************************************/

void*
mm_realloc (void* ptr, uint32_t size)
{
  fprintf (stderr, "realloc block at %p to %u\n", ptr, size);
  return NULL;
}

/****************************************************************/

int
mm_check (void)
{
  address temp = g_heapBase;
  // bool isPrevAlloc = false;
  printf ("Header sentinel tag: %d | %d\n", sizeOf (temp - TAG_SIZE), isAllocated (temp - TAG_SIZE));

  for (; sizeOf (temp) != 0; temp = nextBlock (temp))
  {
    printBlock (temp);
  }

  printf ("Footer sentinel tag: %d | %d\n", sizeOf (temp - TAG_SIZE), isAllocated (temp - TAG_SIZE));
  return 1;
}

/****************************************************************/

static inline address
coalesce (address ptr)
{
  address prev = (address)header (ptr);
  bool alloc_prevBlock = isAllocated (prev);
  bool alloc_nextBlock = isAllocated (nextBlock (ptr));
  uint32_t size = sizeOf (ptr);

  if (alloc_prevBlock && alloc_nextBlock)
  {
    return ptr;
  }
  else if (alloc_prevBlock && !alloc_nextBlock)
  {
    size += sizeOf (nextBlock (ptr));
    makeBlock (ptr, size, 0);
  }
  else if (!alloc_prevBlock && alloc_nextBlock)
  {
    size += sizeOf (prev);
    ptr = prev;
    makeBlock (ptr, size, 0);
  }
  else
  {
    size += (sizeOf (prev) + sizeOf (nextBlock (ptr)));
    ptr = prev;
    makeBlock (ptr, size, 0);
  }

  return ptr;
}

/****************************************************************/

/* returns header address given basePtr */
static inline tag*
header (address p)
{
  return (tag*)(p - TAG_SIZE);
}

/* return true IFF block is allocated */
static inline bool
isAllocated (address p)
{
  return *header (p) & 0x1;
}

/* returns size of block (words) */
static inline uint32_t
sizeOf (address p)
{
  return (*header (p) & (tag)~0x1);
}

/* returns footer address given basePtr */
static inline tag*
footer (address p)
{
  return nextHeader (p) - 1;
}

/* gives the basePtr of next block */
static inline address
nextBlock (address p)
{
  return (address) (nextHeader (p) + 1);
}

/* returns a pointer to the prev block’s
 footer. HINT: you will use header() */
static inline tag*
prevFooter (address p)
{
  return header (p) - 1;
}

/* returns a pointer to the next block’s
 header. HINT: you will use sizeOf() */
static inline tag*
nextHeader (address p)
{
  return (tag*)(p + ((sizeOf (p) * WORD_SIZE) - TAG_SIZE));
}

/* gives the basePtr of prev block */
static inline address
prevBlock (address p)
{
  return p - (sizeOf ((address)header (p)) * WORD_SIZE);
}

/* basePtr, size, allocated */
static inline void
makeBlock (address p, tag t, bool b)
{
  tag* headerTag = header (p);
  *headerTag = t | b;
  tag* footerTag = footer (p);
  *footerTag = t | b;
}

/* basePtr — toggles alloced/free */
static inline void
toggleBlock (address p)
{
  tag* headerTag = header (p);
  *headerTag = (sizeOf (p) | !isAllocated (p));
  tag* footerTag = footer (p);
  *footerTag = (sizeOf (p) | !isAllocated (p));
}

/****************************************************************/
// Testing code
/****************************************************************/

void
printPtrDiff (const char* header, void* p, void* base)
{
  printf ("%s: %td\n", header, (address)p - (address)base);
}

void
printBlock (address p)
{
  printf ("Block Addr %p; Size %u; Alloc %d\n", p, sizeOf (p), isAllocated (p));
}


int
main ()
{
  // Each line is a DWORD
  //           Word      0       1
  //                    ====  ============
  // tag heapZero[] = {
  //                   0, 0, 1    , 4 | 1,
  //                   0, 0, 0    ,     0,
  //                   0, 0, 4 | 1, 2 | 0,
  //                   0, 0, 2 | 0,     1
  //                 };
  tag heapZero[24] = {0};
  // Point to DWORD 1 (DWORD 0 has no space before it)
  g_heapBase = (address)heapZero + DWORD_SIZE;
  // mm_init();
  makeBlock (g_heapBase, 6, 0);
  *prevFooter (g_heapBase) = 0 | 1;
  *nextHeader (g_heapBase) = 0 | 1;
  makeBlock (g_heapBase, 2, 0);
  makeBlock (nextBlock (g_heapBase), 2, 1);
  makeBlock (nextBlock (nextBlock (g_heapBase)), 2, 0);
  /*
  address lastBlock = nextBlock (nextBlock (g_heapBase));
  makeBlock (lastBlock, 4, 1);
  printPtrDiff ("header", header (g_heapBase), heapZero);
  printPtrDiff ("footer", footer (g_heapBase), heapZero);
  printPtrDiff ("nextBlock", nextBlock (g_heapBase), heapZero);
  printPtrDiff ("prevFooter", prevFooter (g_heapBase), heapZero);
  printPtrDiff ("nextHeader", nextHeader (g_heapBase), heapZero);
  address twoWordBlock = nextBlock (g_heapBase);
  printPtrDiff ("prevBlock", prevBlock (twoWordBlock), heapZero);

  // toggleBlock (g_heapBase);
  printf ("%s: %d\n", "isAllocated", isAllocated (g_heapBase));
  printf ("%s: %u\n", "sizeOf Block", sizeOf (g_heapBase));
  printf ("%s: %u\n", "sizeOf nextBlock", sizeOf (twoWordBlock));
  printf ("%s: %u\n", "sizeOf lastBlock", sizeOf (lastBlock));
  */
  //Canonical loop to traverse all blocks
  printf ("All blocks\n");
  for (address p = g_heapBase; sizeOf (p) != 0; p = nextBlock (p))
  {
    printBlock (p);
  }
  mm_free(nextBlock(g_heapBase));
  printf ("\nAll blocks\n");
  for (address p = g_heapBase; sizeOf (p) != 0; p = nextBlock (p))
  {
    printBlock (p);
  }

  return 0;
}
