/*
  Filename   : mm.c
  Author     : Jonathan Wilkins & Nick Loser
  Course     : CSCI 380-01
  Assignment : Assignment 8
  Description: Implements allocation
*/

/*
  The heap "data structure" is made up of a bunch of blocks, each one containing
    a header tag and a footer tag that show the number of words the block takes
    up (including the header and footer tags), and whether or not the block is
    allocated. When allocating new blocks, the program searches for the first
    unallocated block that is large enough to hold the amount of size requested
    by the user and breaks down that block to give the user the size that they
    requested. If a large enough block is not found, the heap space is extended
    to provide one. When freeing a block, it is as simple as flipping the first
    bit in the header and footer tags of that block and then coalescing it with
    surrounding unallocated blocks (combining it with the previous and/or next
    block to create a larger block) to reduce the fragmentation of the heap. If
    we did not do this, the heap would end up containing a bunch of smaller
    blocks that are not big enough to hold larger blocks and we would be wasting
    a lot of the memory that the heap utilizes and blocks off for us to use.
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

// sizeof (word) = 8
const uint8_t WORD_SIZE = 8;
// sizeof (tag) = 4
const uint8_t TAG_SIZE = 4;
// 2 * sizeof (word) = 16
const uint8_t DWORD_SIZE = 16;
// 2 * sizeof (tag) = sizeof (word) = 8
const uint8_t OVERHEAD_BYTES = 8;
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

static inline int
mm_check (void);

static inline address
coalesce (address ptr);

static inline address
extendHeap (address p, uint32_t size);

/****************************************************************/
// Non-inline functions

/*
  mm_init: Initializes the heap so we can begin allocating space for the user

    Returns -1 if heap space could not be allocated or 0 if the heap was set up
      correctly with the header and footer sentinel tags and the initial
      unallocated block
*/
int
mm_init (void)
{
  // Call mem_init to initialize the heap
  mem_init ();
  // Call mem_sbrk to extend the heap to start the heap at 64 bytes of space
  if ((g_heapBase = mem_sbrk (8 * WORD_SIZE)) == (void*)-1)
  {
    return -1;
  }
  // Initialize g_heapBase to point to the first block
  g_heapBase += DWORD_SIZE;
  // Place the sentinel tags and the unallocated block that fills the heap
  *(g_heapBase - WORD_SIZE) = 0 | 1;
  makeBlock (g_heapBase, 6, 0);
  *nextHeader (g_heapBase) = 0 | 1;
  return 0;
}

/****************************************************************/

/*
  mm_malloc: Allocates a block of at least the requested size for the user
  
    Returns NULL if the given size was less than or equal to 0 or if the heap
      was unable to be extended in order to provide a larger sized block.
      Otherwise, returns a pointer to the allocated block
*/
void*
mm_malloc (uint32_t size)
{
  // If size is <= 0, return null immediately to save time
  if (size <= 0)
  {
    return NULL;
  }
  // Calculate the number of words needed to allocate to hold 'size' bytes
  uint32_t actSize =
    (((size + OVERHEAD_BYTES) + (DWORD_SIZE - 1)) / DWORD_SIZE) * 2;
  address p = g_heapBase;
  // Search for the first unallocated block that can hold 'actSize' words
  for (; sizeOf (p) != 0; p = nextBlock (p))
  {
    // If the block is already allocated, don't bother checking anything else with it
    if (isAllocated (p))
      continue;
    // See the new block fits in the free block at p
    if (actSize <= sizeOf (p))
    {
      // Make the new block into the free block at p
      uint32_t psize = sizeOf (p);
      makeBlock (p, actSize, 1);
      // If the free block has more leftover space after inserting the new block,
      //  create a new free block to hold the leftover space
      if (actSize < psize)
        makeBlock (nextBlock (p), psize - actSize, 0);
      return p;
    }
  }

  // Extend the heap to fit the requested size
  p = extendHeap (p, actSize);
  // If it's not null, allocate the requested size and return the pointer to it
  if (p != NULL)
  {
    makeBlock (p, actSize, 1);
    return p;
  }
  return NULL;
}

/****************************************************************/

/*
  mm_free: Unallocates the block at the given pointer and combines the newly
    freed block with surrounding unallocated blocks

    Returns nothing. However, it exits early if the given pointer does not point
      anywhere, the block is not allocated, or mm_init has not been called first
*/
void
mm_free (void* ptr)
{
  // Make sure ptr actually points somewhere, the block is not allocated, and the
  //  heap exists
  if (ptr == 0 || !isAllocated (ptr) || g_heapBase == 0)
  {
    return;
  }

  // "Unallocate" the block just by toggling it
  toggleBlock (ptr);
  // Coalesce the block at ptr with surrounding blocks
  coalesce (ptr);
}

/****************************************************************/

/*
  mm_realloc: Reallocates the block at the given pointer with the given size

    Returns NULL and frees the block if the size is 0, and returns a pointer to
      the new block with the data from the old block copied into the new one
*/
void*
mm_realloc (void* ptr, uint32_t size)
{
  // If ptr is null, just call mm_malloc with the given size
  if (ptr == NULL)
  {
    return mm_malloc (size);
  }
  // If the size is 0, just call mm_free with the given ptr
  else if (size == 0)
  {
    mm_free (ptr);
    return NULL;
  }

  // Malloc a new block to give the user
  address newPtr = mm_malloc (size);
  if (!newPtr)
  {
    return NULL;
  }

  // Calculate the size of the old block
  uint32_t oldSize = sizeOf (ptr) * WORD_SIZE;
  // Get the smaller size of size and oldSize so we don't copy too much
  if (size < oldSize)
  {
    oldSize = size;
  }
  // Copy the data from the old block to the new block
  memcpy (newPtr, ptr, oldSize);
  // Free the old block
  mm_free (ptr);
  return newPtr;
}

/****************************************************************/

/*
  mm_check: Checks the heap to make sure its structure and composition are valid
  
    Returns 0 and prints and error message depending on the issue with the heap,
      or returns 1 if the heap is correctly formed
*/
static inline int
mm_check (void)
{
  // Check if header sentinel tag is correct
  address afterSentTag = (address)header (g_heapBase);
  if (sizeOf (afterSentTag) != 0 || isAllocated (afterSentTag) != 1)
  {
    fprintf (stderr, "Beginning sentinel tag is not set up correctly\n");
    fprintf (stderr, "Expected: %u | %u - Actual: %u | %u\n", 0, 1,
              sizeOf (afterSentTag), isAllocated (afterSentTag));
    return 0;
  }

  address temp = g_heapBase;
  for (; sizeOf (temp) != 0; temp = nextBlock (temp))
  {
    // Check if block is aligned to a DWORD
    if ((uint64_t)temp % DWORD_SIZE)
    {
      fprintf (stderr, "Block at address %p is not aligned correctly\n", temp);
      fprintf (stderr, "Block is %lu bytes off of a double word boundary\n",
                (uint64_t)temp % DWORD_SIZE);
      return 0;
    }
    // Check if header and footer tags match
    if ((*header (temp)) != (*footer (temp)))
    {
      fprintf (stderr, "Header and footer tag for block address %p do not match\n", temp);
      fprintf (stderr, "Header: %u | %u - Footer: %u | %u\n", sizeOf (temp),
                isAllocated (temp), sizeOf ((address)nextHeader (temp)),
                isAllocated ((address)nextHeader (temp)));
      return 0;
    }
    // Check if coalescing is correct
    if (!isAllocated (temp))
    {
      bool alloc_prevBlock = isAllocated ((address)header (temp));
      bool alloc_nextBlock = isAllocated (nextBlock (temp));
      // Print the general error message for coalescing once instead of in every case
      if (!alloc_nextBlock || !alloc_prevBlock)
        fprintf (stderr, "Block at address %p was not coalesced correctly\n", temp);
      if (alloc_prevBlock && !alloc_nextBlock)
      {
        // Triggers when this block and next block are both not allocated
        fprintf (stderr, "Should have coalesced with next block\n");
        return 0;
      }
      else if (!alloc_prevBlock && alloc_nextBlock)
      {
        // Triggers when this block and previous block are both not allocated
        // Should never reach here because the previous block would trigger the
        //  alloc_prevBlock && !alloc_nextBlock case and return, or the sentinel
        //  block would not be correct and the check for the header sentinel tag
        //  would have already triggered and returned
        fprintf (stderr, "Should have coalesced with previous block\n");
        return 0;
      }
      else if (!alloc_prevBlock && !alloc_nextBlock)
      {
        // Triggers when this block, the previous block, and the next block are
        //  not allocated
        // Should never reach here because the previous block would trigger the
        //  alloc_prevBlock && !alloc_nextBlock case and return
        fprintf (stderr, "Should have coalesced with both surrounding blocks\n");
        return 0;
      }
    }
  }

  // Check footer sentinel tag is correct
  if (sizeOf (temp) != 0 || isAllocated (temp) != 1)
  {
    fprintf (stderr, "Ending sentinel tag is not set up correctly\n");
    fprintf (stderr, "Expected: %u | %u - Actual: %u | %u\n", 0, 1,
              sizeOf (temp), isAllocated (temp));
    return 0;
  }
  return 1;
}

/****************************************************************/

/*
  coalesce: Combines the freed block at the given pointer with other surrounding
    unallocated blocks if possible

    Returns a pointer to the beginning of the combined blocks, or returns the
      same pointer that was given if no surrounding blocks are unallocated
*/
static inline address
coalesce (address ptr)
{
  // Get the address of the header so we can get isAllocated and sizeOf from the
  //  previous block's footer instead of its header
  address prev = (address)header (ptr);
  // Save whether the previous and next blocks are allocated and the size of ptr
  //  to save time from calling isAllocated a bunch of times
  bool alloc_prevBlock = isAllocated (prev);
  bool alloc_nextBlock = isAllocated (nextBlock (ptr));
  uint32_t size = sizeOf (ptr);

  // If both surrounding blocks are allocated, do nothing
  if (alloc_prevBlock && alloc_nextBlock)
  {
    return ptr;
  }
  // If the next block is unallocated, merge this block with the next block
  else if (alloc_prevBlock && !alloc_nextBlock)
  {
    size += sizeOf (nextBlock (ptr));
    makeBlock (ptr, size, 0);
  }
  // If the previous block is unallocated, merge this block with the previous
  //  block
  else if (!alloc_prevBlock && alloc_nextBlock)
  {
    size += sizeOf (prev);
    ptr = prevBlock (ptr);
    makeBlock (ptr, size, 0);
  }
  // Otherwise, both surrounding blocks are unallocated so merge all three
  //  blocks together
  else
  {
    size += (sizeOf (prev) + sizeOf (nextBlock (ptr)));
    ptr = prevBlock (ptr);
    makeBlock (ptr, size, 0);
  }
  return ptr;
}

/****************************************************************/

/*
  extendHeap: Extends the heap to create blocks for larger blocks that didn't fit
    anywhere else in the heap

    Returns a pointer to the beginning of the newly added heap space, or returns
      NULL if new space couldn't be added to the heap
*/
static inline address
extendHeap (address p, uint32_t size)
{
  // Record the previous block's size and the DWORD aligned size
  uint32_t prevSize = sizeOf (prevBlock (p));
  uint32_t asize = isAllocated (prevBlock (p)) ? size : size - prevSize;
  // Call mem_sbrk to physically extend the heap
  if ((p = mem_sbrk ((int)(asize * WORD_SIZE))) == (void*)-1)
    return NULL;
  // Make an unallocated block in the new heap space
  makeBlock (p, asize, 0);
  // Set the sentinel footer tag at the end of the new heap space
  *nextHeader (p) = 0 | 1;
  // Call coalesce in case the last block of the old heap space is unallocated
  return coalesce (p);
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
  return (tag*)(nextBlock (p) - WORD_SIZE);
}

/* gives the basePtr of next block */
static inline address
nextBlock (address p)
{
  return p + (sizeOf (p) * WORD_SIZE);
}

/* returns a pointer to the prev block’s
 footer. HINT: you will use header() */
static inline tag*
prevFooter (address p)
{
  return (tag*)(p - WORD_SIZE);
}

/* returns a pointer to the next block’s
 header. HINT: you will use sizeOf() */
static inline tag*
nextHeader (address p)
{
  return (tag*)(nextBlock (p) - TAG_SIZE);
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
  *header (p) = t | b;
  *footer (p) = t | b;
}

/* basePtr — toggles alloced/free */
static inline void
toggleBlock (address p)
{
  *header (p) = (sizeOf (p) | !isAllocated (p));
  *footer (p) = (sizeOf (p) | isAllocated (p));
}