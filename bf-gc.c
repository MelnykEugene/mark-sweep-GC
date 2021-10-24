// ==============================================================================
/**
 * bf-gc.c
 **/
// ==============================================================================



// ==============================================================================
// INCLUDES

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "gc.h"
#include "safeio.h"
// ==============================================================================



// ==============================================================================
// TYPES AND STRUCTURES

/** The header for each allocated object. */
typedef struct header {

  /** Pointer to the next header in the list. */
  struct header* next;

  /** Pointer to the previous header in the list. */
  struct header* prev;

  /** The usable size of the block (exclusive of the header itself). */
  size_t         size;

  /** Is the block allocated or free? */
  bool           allocated;

  /** Whether the block has been visited during reachability analysis. */
  bool           marked;

  /** A map of the layout of pointers in the object. */
  gc_layout_s*   layout;

} header_s;

/** A link in a linked stack of pointers, used during heap traversal. */
typedef struct ptr_link {

  /** The next link in the stack. */
  struct ptr_link* next;

  /** The pointer itself. */
  void* ptr;

} ptr_link_s;
// ==============================================================================



// ==============================================================================
// MACRO CONSTANTS AND FUNCTIONS

/** The system's page size. */
#define PAGE_SIZE sysconf(_SC_PAGESIZE)

/**
 * Macros to easily calculate the number of bytes for larger scales (e.g., kilo,
 * mega, gigabytes).
 */
#define KB(size)  ((size_t)size * 1024)
#define MB(size)  (KB(size) * 1024)
#define GB(size)  (MB(size) * 1024)

/** The virtual address space reserved for the heap. */
#define HEAP_SIZE GB(2)

/** Given a pointer to a header, obtain a `void*` pointer to the block itself. */
#define HEADER_TO_BLOCK(hp) ((void*)((intptr_t)hp + sizeof(header_s)))

/** Given a pointer to a block, obtain a `header_s*` pointer to its header. */
#define BLOCK_TO_HEADER(bp) ((header_s*)((intptr_t)bp - sizeof(header_s)))
// ==============================================================================


// ==============================================================================
// GLOBALS

/** The address of the next available byte in the heap region. */
static intptr_t free_addr  = 0;

/** The beginning of the heap. */
static intptr_t start_addr = 0;

/** The end of the heap. */
static intptr_t end_addr   = 0;

/** The head of the free list. */
static header_s* free_list_head = NULL;

/** The head of the allocated list. */
static header_s* allocated_list_head = NULL;

/** The head of the root set stack. */
static ptr_link_s* root_set_head = NULL;
// ==============================================================================



// ==============================================================================
/**
 * Push a pointer onto root set stack.
 *
 * \param ptr The pointer to be pushed.
 */
void rs_push (void* ptr) {

  // Make a new link.
  ptr_link_s* link = malloc(sizeof(ptr_link_s));
  if (link == NULL) {
    ERROR("rs_push(): Failed to allocate link");
  }

  // Have it store the pointer and insert it at the front.
  link->ptr    = ptr;
  link->next   = root_set_head;
  root_set_head = link;
  
} // rs_push ()
// ==============================================================================



// ==============================================================================
/**
 * Pop a pointer from the root set stack.
 *
 * \return The top pointer being removed, if the stack is non-empty;
 *         <code>NULL</code>, otherwise.
 */
void* rs_pop () {

  // Grab the pointer from the link...if there is one.
  if (root_set_head == NULL) {
    return NULL;
  }
  void* ptr = root_set_head->ptr;

  // Remove and free the link.
  ptr_link_s* old_head = root_set_head;
  root_set_head = root_set_head->next;
  free(old_head);

  return ptr;
  
} // rs_pop ()
// ==============================================================================



// ==============================================================================
/**
 * Add a pointer to the _root set_, which are the starting points of the garbage
 * collection heap traversal.  *Only add pointers to objects that will be live
 * at the time of collection.*
 *
 * \param ptr A pointer to be added to the _root set_ of pointers.
 */
void gc_root_set_insert (void* ptr) {

  rs_push(ptr);
  
} // root_set_insert ()
// ==============================================================================



// ==============================================================================
/**
 * The initialization method.  If this is the first use of the heap, initialize it.
 */

void gc_init () {

  // Only do anything if there is no heap region (i.e., first time called).
  if (start_addr == 0) {

    DEBUG("Trying to initialize");
    
    // Allocate virtual address space in which the heap will reside. Make it
    // un-shared and not backed by any file (_anonymous_ space).  A failure to
    // map this space is fatal.
    void* heap = mmap(NULL,
		      HEAP_SIZE,
		      PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS,
		      -1,
		      0);
    if (heap == MAP_FAILED) {
      ERROR("Could not mmap() heap region");
    }

    // Hold onto the boundaries of the heap as a whole.
    start_addr = (intptr_t)heap;
    end_addr   = start_addr + HEAP_SIZE;
    free_addr  = start_addr;

    // DEBUG: Emit a message to indicate that this allocator is being called.
    DEBUG("bf-alloc initialized");

  }

} // gc_init ()
// ==============================================================================


// ==============================================================================
// COPY-AND-PASTE YOUR PROJECT-4 malloc() HERE.
//
//   Note that you may have to adapt small things.  For example, the `init()`
//   function is now `gc_init()` (above); the header is a little bit different
//   from the Project-4 one; my `allocated_list_head` may be a slightly
//   different name than the one you used.  Check the details.
void* gc_malloc (size_t size) {


} // gc_malloc ()
// ==============================================================================



// ==============================================================================
// COPY-AND-PASTE YOUR PROJECT-4 free() HERE.
//
//   See above.  Small details may have changed, but the code should largely be
//   unchanged.
void gc_free (void* ptr) {

  if (ptr == NULL) {
    return;
  }

  header_s* header_ptr = BLOCK_TO_HEADER(ptr);

  if (!header_ptr->allocated) {
    ERROR("Double-free: ", (intptr_t)header_ptr);  //if the given block wasn't marked as allocated, this means we are trying to free an unallocated block.
  }

  if(header_ptr->prev==NULL){
    allocated_list_head=header_ptr->next;
  } else{
    header_ptr->prev->next = header_ptr->next;
  }

  if(header_ptr->next!=NULL){
    header_ptr->next->prev = header_ptr->prev;
  }

  header_ptr->next=NULL; //probably unnecessary?
  header_ptr->prev=NULL;

  header_ptr->next = free_list_head;
  free_list_head = header_ptr;

  if(header_ptr->next!=NULL){
    header_ptr->next->prev=header_ptr;
    }

  header_ptr->allocated = false;
  
} // gc_free ()
// ==============================================================================



// ==============================================================================
/**
 * Allocate and return heap space for the structure defined by the given
 * `layout`.
 *
 * \param layout A descriptor of the fields
 * \return A pointer to the allocated block, if successful; `NULL` if unsuccessful.
 */

void* gc_new (gc_layout_s* layout) {

  // Get a block large enough for the requested layout.
  void*     block_ptr  = gc_malloc(layout->size);
  header_s* header_ptr = BLOCK_TO_HEADER(block_ptr);

  // Hold onto the layout for later, when a collection occurs.
  header_ptr->marked = false;
  header_ptr->layout = layout;
  
  return block_ptr;
  
} // gc_new ()
// ==============================================================================



// ==============================================================================
/**
 * Traverse the heap, marking all live objects.
 */

void mark () {
 while(!root_set_head==NULL){
  void* current_block = rs_pop();

  if (current_block !=NULL){ //this check might be excessive, idc
    header_s* current_header = BLOCK_TO_HEADER(current_block);

    header->marked = true;

    gc_layout_s* current_layout = current_header->layout;
    size_t* offsets = current_layout->ptr_offsets;
    for (int i =0; i< current_layout->num_ptrs; i++){
-      void** handle = current_block + offsets[i];
      rs_push(*handle);
    }
  }
 }
} // mark ()
// ==============================================================================



// ==============================================================================
/**
 * Traverse the allocated list of objects.  Free each unmarked object;
 * unmark each marked object (preparing it for the next sweep.
 */

void sweep () {

header_s* current_header = allocated_list_head;
while (current_header!=NULL){
  header_s* next_header = current_header->next;
  void* current_block = HEADER_TO_BLOCK(current_header);
  if (current_header->marked==true){
    current_header->marked=false;
  }
  else{
    gc_free(current_block);
  }
  current_header=next_header;
 }

} // sweep ()
// ==============================================================================


// ==============================================================================
/**
 * Garbage collect the heap.  Traverse and _mark_ live objects based on the
 * _root set_ passed, and then _sweep_ the unmarked, dead objects onto the free
 * list.  This function empties the _root set_.
 */

void gc () {

  // Traverse the heap, marking the objects visited as live.
  mark();

  // And then sweep the dead objects away.
  sweep();

  // Sanity check:  The root set should be empty now.
  assert(root_set_head == NULL);
  
} // gc ()
// ==============================================================================
