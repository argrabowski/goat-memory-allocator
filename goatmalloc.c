#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "goatmalloc.h"

int statusno = ERR_UNINITIALIZED; // initialize status number to uninitialized
void* arena_start; // arena start pointer
int arena_size = 0; // arena size
node_t* chunkList = NULL; // chunk list set to NULL

void split(node_t* node, size_t size); // split helper function prototype
void merge(node_t* curr); // merge helper function prototype

int init(size_t size) // initialization function
{
  printf("Initializing Arena:\n");
  int sizeI = (int)size; // get integer form of size
  if(sizeI < 0) // if the size is less than 0
  {
    printf("...bad arguments error\n");
    statusno = ERR_UNINITIALIZED;
    return ERR_BAD_ARGUMENTS;
  }
  printf("...requested size %ld bytes\n", size);
  int page_size = getpagesize(); // get the page size
  if(page_size <= 0) // if the call failed
  {
    statusno = ERR_CALL_FAILED;
    return statusno;
  }
  printf("...pagesize is %d bytes\n", page_size);
  printf("...adjusting size with page boundaries\n");
  arena_size = size - (size % page_size) + page_size * !!(size % page_size); // calculate the arena size
  printf("...adjusted size is %d bytes\n", arena_size);
  int fd = open("/dev/zero", O_RDWR); // open for read and write
  if(fd == -1) // if the call failed
  {
    statusno = ERR_SYSCALL_FAILED;
    return statusno;
  }
  printf("...mapping arena with mmap()\n");
  arena_start = mmap(NULL, arena_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0); // get the start of the arena with mmap
  if(arena_start == MAP_FAILED) // if the call failed
  {
    statusno = ERR_SYSCALL_FAILED;
    return statusno;
  }
  printf("...arena starts at %p\n", arena_start);
  printf("...arena ends at %p\n", arena_start + arena_size);
  printf("...initializing header for initial free chunk\n");
  chunkList = arena_start; // set the chunk list to the start of the arena
  chunkList->size = arena_size - sizeof(node_t); // set the size of the first header
  chunkList->is_free = 1; // the first header is free
  chunkList->fwd = NULL; // first header has no forward
  chunkList->bwd = NULL; // first header has no backward
  printf("...header size is %ld bytes\n", sizeof(node_t));
  statusno = 0;
  return arena_size; // return the size of the arena
}

int destroy()
{
  printf("Destroying Arena:\n");
  if(statusno == ERR_UNINITIALIZED) // if arena has not been initialized
  {
    printf("...uninitialized error\n");
    return statusno;
  }
  printf("...unmapping arena with munmap()\n");
  int ret_val = munmap(arena_start, arena_size); // unmap the arena
  if(ret_val == -1) // if the call failed
  {
    statusno = ERR_SYSCALL_FAILED;
    return statusno;
  }
  arena_size = 0; // reset the arena size
  chunkList = NULL; // reset the chunk list
  statusno = 0; // reset the status number
  return ret_val; // return 0 on success
}

void* walloc(size_t size)
{
  printf("Allocating Memory:\n");
  if(statusno == ERR_UNINITIALIZED) // if the arena has not been initialized
  {
    printf("...uninitialized error\n");
    return NULL;
  }
  void* result; // result pointer
  node_t* curr = chunkList; // current node
  printf("...looking for free chunk of >= %ld bytes\n", size);
  while((curr->size < size || curr->is_free == 0) && curr->fwd != NULL) // find a free node thats big enough
  {
    curr = curr->fwd;
  }
  if(curr->is_free == 1 && (curr->size == size || curr->size > size)) // if the node is free and big enough
  {
    printf("...found free chunk of %ld bytes with header at %p\n", curr->size, curr);
    printf("...free chunk->fwd currently points to %p\n", curr->fwd);
    printf("...free chunk->bwd currently points to %p\n", curr->bwd);
    printf("...checking if splitting is required\n");
    if(curr->size == size) // if the current node fits perfectly
    {
      printf("...splitting is not required\n");
      printf("...updating chunk header at %p\n", curr);
      curr->is_free = 0; // set the current node to allocated
      printf("...being careful with pointer arithmetic and void pointer casting\n");
      result = (void*)(++curr); // get pointer to the allocation
      printf("...allocation starts at %p\n", result);
      return result; // return pointer to the allocation
    }
    else if(curr->size >= size + sizeof(node_t)) // if the allocated memory is smaller than the free chunk
    {
      printf("...splitting is required\n");
      printf("...updating chunk header at %p\n", curr);
      split(curr, size); // split the current node
      printf("...being careful with pointer arithmetic and void pointer casting\n");
      result = (void*)(++curr); // get pointer to the allocation
      printf("...allocation starts at %p\n", result);
      return result; // return pointer to the allocation
    }
    else // if there is no more room in the arena to split
    {
      printf("...splitting not possible\n");
      printf("...updating chunk header at %p\n", curr);
      curr->is_free = 0; // set the current node to allocated
      printf("...being careful with pointer arithmetic and void pointer casting\n");
      result = (void*)(++curr); // get pointer to the allocation
      printf("...allocation starts at %p\n", result);
      return result; // return pointer to the allocation
    }
  }
  else // if there is no room in the arena for the allocation
  {
    printf("...out of memory error\n");
    statusno = ERR_OUT_OF_MEMORY;
    return NULL;
  }
}

void wfree(void *ptr)
{
  printf("Freeing Allocated Memory:\n");
  printf("...supplied pointer %p\n", ptr);
  if(arena_start <= ptr && ptr <= arena_start + arena_size) // if the pointer is within the arena
  {
    node_t* curr = ptr; // set the current node equal to the pointer passed
    printf("...being careful with pointer arithmetic and void pointer casting\n");
    --curr; // decrement to get pointer
    printf("...accessing chunk header at %p\n", curr);
    printf("...chunk is of size %ld\n", curr->size);
    curr->is_free = 1; // set the current node to free
    printf("...checking if coalescing is needed\n");
    merge(curr); // go through and check for coalescing
  }
  else // if the pointer is not in the arena
  {
    printf("...pointer not valid error\n");
  }
}

void split(node_t* node, size_t size)
{
  node_t* new = (void*)((void*)node + size + sizeof(node_t)); // set the location of the new node
  new->size = node->size - size - sizeof(node_t); // set the size of the new node
  new->is_free = 1; // set the new node to be free
  new->fwd = NULL; // new node does not have forward
  new->bwd = node; // the previous node is behind the new node
  node->size = size; // previous node size is passed as size
  node->is_free = 0; // previous node is allocated
  node->fwd = new; // previous node forward is the new node
}

void merge(node_t* curr)
{
  int col = 0; // keeps track of which coalescing case is used
  if(curr->bwd && curr && curr->fwd) // if current, forward, and backward nodes exist
  {
    if(curr->bwd->is_free && curr->is_free && curr->fwd->is_free) // if those nodes are free
    {
      col = 1;
      printf("...coalescing needed\n");
      printf("...col. case %d: previous, current, and next chunks free\n", col);
      curr = curr->bwd; // set current to previous
      curr->size += curr->fwd->size + curr->fwd->fwd->size + (sizeof(node_t) * 2); // add up 3 nodes with 2 headers
      curr->fwd = curr->fwd->fwd->fwd; // set the forward to be the original fordard's forward
    }
    else if(curr->bwd->is_free && curr->is_free) // if backward and current nodes are free
    {
      col = 2;
      printf("...coalescing needed\n");
      printf("...col. case %d: previous and current chunks free\n", col);
      curr = curr->bwd; // set current to previous
      curr->size += curr->fwd->size + sizeof(node_t); // add up 2 nodes with header
      curr->fwd = curr->fwd->fwd; // set the forward to be the original fordard
    }
    else if(curr->is_free && curr->fwd->is_free) // if current and forward nodes are free
    {
      col = 3;
      printf("...coalescing needed\n");
      printf("...col. case %d: current and next chunks free\n", col);
      curr->size += curr->fwd->size + sizeof(node_t); // add up 2 nodes with header
      curr->fwd = curr->fwd->fwd; // set forward to be forward's forward
    }
  }
  else if(curr->bwd && curr) // if backward and current nodes exist
  {
    if(curr->bwd->is_free && curr->is_free) // if those nodes are free
    {
      col = 2;
      printf("...coalescing needed\n");
      printf("...col. case %d: previous and current chunks free\n", col);
      curr = curr->bwd; // set current to previous
      curr->size += curr->fwd->size + sizeof(node_t); // add up 2 nodes with header
      curr->fwd = curr->fwd->fwd; // set the forward to be the original fordard
    }
  }
  else if(curr && curr->fwd) // if current and forward nodes exist
  {
    if(curr->is_free && curr->fwd->is_free) // if those nodes are free
    {
      col = 3;
      printf("...coalescing needed\n");
      printf("...col. case %d: current and next chunks free\n", col);
      curr->size += curr->fwd->size + sizeof(node_t); // add up 2 nodes with header
      curr->fwd = curr->fwd->fwd; // set forward to be forward's forward
    }
  }
  if(col == 0) // if coalescing is not required
  {
    printf("...coalescing not needed\n");
  }
}
