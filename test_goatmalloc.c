#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "goatmalloc.h"

void print_header(node_t *header){
  //Note: These printf statements may produce a segmentation fault if the buff
  //pointer is incorrect, e.g., if buff points to the start of the arena.
  printf("Header->size: %lu\n", header->size);
  printf("Header->fwd: %p\n", header->fwd);
  printf("Header->bwd: %p\n", header->bwd);
  printf("Header->is_free: %d\n", header->is_free);
}

void test_init_destroy(){
  int arena_size = 0;
  int page_size = getpagesize();
  void *buff = NULL;
  node_t *header = NULL;

  printf(">> Testing initialization.\n");

  // The init function must adjust the arena size to be a multiple of pagesize.
  arena_size = init(1);
  assert(arena_size == page_size);
  assert(destroy() == 0);

  arena_size = init((page_size*2)-10);
  assert(arena_size == 2*page_size);
  assert(destroy() == 0);

  arena_size = init(page_size);
  assert(arena_size == page_size);
  assert(destroy() == 0);

  // The init function must also check for error conditions and bad arguments.
  assert(init(-1) == ERR_BAD_ARGUMENTS);
  assert(destroy() == ERR_UNINITIALIZED);
}

void test_allocation_basic(){
  int size = 0;
  int page_size = getpagesize();
  void *buff, *buff2 = NULL;
  node_t *header = NULL;

  printf(">> Testing allocations without the possibility to split. No Frees.\n");

  buff = walloc(page_size);
  assert(statusno == ERR_UNINITIALIZED);
  assert(buff == NULL);

  // Allocation not possible because we didn't account for the header which is
  // also placed in the arena and takes of sizeof(node_t) bytes.
  init(page_size);
  buff = walloc(page_size);
  assert(statusno == ERR_OUT_OF_MEMORY);
  assert(buff == NULL);

  size = page_size-sizeof(node_t);
  buff = walloc(size);
  assert(buff != NULL);

  // Check that we can write to the allocated memory
  memset(buff, 'a', size);
  assert(((char *)buff)[0] == 'a' && ((char *)buff)[size-1] == 'a');

  //This allocation should fail because the previous allocation used all of
  //the remaining space.
  buff2 = walloc(1);
  assert(buff2 == NULL);
  assert(statusno == ERR_OUT_OF_MEMORY);

  destroy();
}

void test_allocationfree_placement(){
  int size = 0;
  int page_size = getpagesize();
  void *buff, *buff2,  *buff3;
  node_t *header = NULL;

  printf(">> Testing basic allocation free placement.\n");

  // Test: Create two allocatations.
  // Free first allocation but leave the second allocation in place.
  // Create third allocation an Ensure that the
  // third allocation is placed in the first free chunk
  init(page_size);

  size = 32;
  buff = walloc(size);
  assert(buff != NULL);
  memset(buff, 'a', size);

  header = (node_t *)(buff - sizeof(node_t));

  //This checks that the header for the first allocation is placed
  //at the start of a page boundary. If this check fails, then the
  //first header is not placed at the start of the arena.
  assert(((unsigned long)header & 0xFFF) == 0);

  print_header(header);

  buff2 = walloc(size);

  wfree(buff);

  //Next allocation should be in same place as the first allocation
  buff3 = walloc(size);
  assert(buff3 == buff);
  //The characters from the previous memset should also be there
  assert(((char *)buff)[0] == 'a' && ((char *)buff)[size-1] == 'a');

  destroy();
}

void test_allocation_withsplits(){
  int size = 0;
  int page_size = getpagesize();
  void *buff = NULL, *buff2 = NULL;
  node_t *header = NULL, *header2 = NULL;

  printf(">>Testing allocations with splits.\n");

  // Test: First allocation causes split. Check that the header is in the
  // correct place, has the correct field values, and that chunks were split
  // correctly
  init(page_size);
  buff = walloc(64);
  assert(buff != NULL);

  header = (node_t *)(buff - sizeof(node_t));
  print_header(header);

  assert(header->size == 64);
  assert(header->bwd == NULL);
  assert(header->is_free == 0);
  assert(header->fwd == ((void*)header)+sizeof(node_t)+64);

  node_t *next = header->fwd;
  print_header(next);

  assert(next->fwd == NULL);
  assert(next->bwd == header);
  assert(next->size == page_size-64-(sizeof(node_t)*2));
  assert(next->is_free == 1);

  // Test: Second allocation uses up remaining free space. Check that
  // the allocation is in the correct place and has the correct field values.
  size = page_size - 64 - (sizeof(node_t) * 2);
  buff2 = walloc(size);

  header2 = (node_t *)(buff2 - sizeof(node_t));
  print_header(header2);

  assert(header2 == next);
  assert(header2->size == size);
  assert(header2->is_free == 0);
  assert(header2->fwd == NULL);
  assert(header2->bwd == header);

  destroy();

	// Test: Only split if the process of splitting would leaves
	// enough room for another chunk.
	init(page_size);
  buff = walloc(64);
	//This should leave 10 bytes remaining in the arena
  size = page_size - 64 - (sizeof(node_t) * 2) - 10;
	buff2 = walloc(size);

  header2 = (node_t *)(buff2 - sizeof(node_t));
  print_header(header2);

  assert(header2->size == size + 10);
  assert(header2->fwd == NULL);

  destroy();
}

void test_free_basic(){
  int size;
  int page_size = getpagesize();
  void *buff;
  node_t *header;

  printf(">>Testing frees without coalescing.\n");
  init(page_size);
  size = page_size - sizeof(node_t);
  buff = walloc(size);
  header = (node_t *)(buff - sizeof(node_t));

  assert(header->is_free == 0);
  wfree(buff);
  assert(header->is_free == 1);
  assert(header->size == size);

  destroy();
}

void test_free_coalescing_case3(){
  int size, size2;
  int page_size = getpagesize();
  void *buff, *buff2;
  node_t *header, *header2;

  printf(">>Testing frees with coalescing. Case 3.\n");

  init(page_size);

  size=64;
  //Have this allocation fill up the rest of the arena
  size2=page_size - size - sizeof(node_t)*2;

  buff = walloc(size);
  buff2 = walloc(size2);

  assert(buff != NULL);
  assert(buff2 != NULL);

  header = ((void*)buff) - sizeof(node_t);
  header2 = ((void*)buff2) - sizeof(node_t);

  print_header(header);
  print_header(header2);

  assert(header->is_free == 0);
  assert(header2->is_free == 0);

  wfree(buff2);
  assert(header2->is_free == 1);
  assert(header2->size == size2);

  //this should cause coalescing.
  wfree(buff);

  //The coalesced node header should reside at the start of the
  //arena (i.e., where header points)
  print_header(header);

  assert(header->size == page_size - sizeof(node_t));
  assert(header->bwd == NULL);
  assert(header->fwd == NULL);

  destroy();
}

void test_free_coalescing_case2(){
  int size, size2;
  int page_size = getpagesize();
  void *buff, *buff2;
  node_t *header, *header2;

  printf(">>Testing frees with coalescing. Case 2.\n");

  init(page_size);

  size=64;
  //Have this allocation fill up the rest of the arena
  size2=page_size - size - sizeof(node_t)*2;

  buff = walloc(size);
  buff2 = walloc(size2);

  assert(buff != NULL);
  assert(buff2 != NULL);

  header = ((void*)buff) - sizeof(node_t);
  header2 = ((void*)buff2) - sizeof(node_t);

  print_header(header);
  print_header(header2);

  assert(header->is_free == 0);
  assert(header2->is_free == 0);

  wfree(buff);
  assert(header->is_free == 1);
  assert(header->size == size);

  //this should cause coalescing.
  wfree(buff2);

  //The coalesced node header should reside at the start of the
  //arena (i.e., where header points)
  print_header(header);

  assert(header->size == page_size - sizeof(node_t));
  assert(header->bwd == NULL);
  assert(header->fwd == NULL);

  destroy();
}

void test_free_coalescing_case1(){
  int size, size2, size3;
  int page_size = getpagesize();
  void *buff, *buff2, *buff3;
  node_t *header, *header2, *header3;

  printf(">>Testing frees with coalescing. Case 1.\n");

  init(page_size);

  size=64;
  size2=128;
  //Have the third allocation fill up the rest of the arena
  size3=page_size - size - size2 - sizeof(node_t)*3;

  //Case 1: Combine prev, current, next
  buff = walloc(size);
  buff2 = walloc(size2);
  buff3 = walloc(size3);

  assert(buff != NULL);
  assert(buff2 != NULL);
  assert(buff3 != NULL);

  header = ((void*)buff) - sizeof(node_t);
  header2 = ((void*)buff2) - sizeof(node_t);
  header3 = ((void*)buff3) - sizeof(node_t);

  print_header(header);
  print_header(header2);
  print_header(header3);

  assert(header->is_free == 0);
  assert(header2->is_free == 0);
  assert(header3->is_free == 0);

  wfree(buff);
  wfree(buff3);

  assert(header->is_free == 1);
  assert(header3->is_free == 1);

  assert(header->size == size);
  assert(header3->size == size3);

  //this should cause coalescing.
  wfree(buff2);

  //The coalesced node header should reside at the start of the
  //arena (i.e., where header points)
  print_header(header);

  assert(header->size == page_size - sizeof(node_t));
  assert(header->bwd == NULL);
  assert(header->fwd == NULL);

  destroy();
}

void test_free_coalescing_chains_fwd(){
  int size;
  int page_size = getpagesize();
  void *buff, *buff2, *buff3, *buff4;

  printf(">>Testing frees with coalescing chaining\n");

  init(page_size);

  size=64;

  buff = walloc(size);
  buff2 = walloc(size);
  buff3 = walloc(size);
  buff4 = walloc(size);

  assert(buff != NULL);
  assert(buff2 != NULL);
  assert(buff3 != NULL);
  assert(buff4 != NULL);

  node_t *header = ((void*)buff) - sizeof(node_t);

  wfree(buff);
  wfree(buff2);
  wfree(buff3);
  wfree(buff4);

  print_header(header);

  //check to make sure the above seq. ends with
  //a single free node.
  assert(header->size == page_size - sizeof(node_t));
  assert(header->is_free == 1);
  assert(header->bwd == NULL);
  assert(header->fwd == NULL);

  destroy();
}

void test_free_coalescing_chains_bwd(){
  int size;
  int page_size = getpagesize();
  void *buff, *buff2, *buff3, *buff4;

  printf(">>Testing frees with coalescing chaining\n");

  init(page_size);

  size=64;

  buff = walloc(size);
  buff2 = walloc(size);
  buff3 = walloc(size);
  buff4 = walloc(size);

  assert(buff != NULL);
  assert(buff2 != NULL);
  assert(buff3 != NULL);
  assert(buff4 != NULL);

  node_t *header = ((void*)buff) - sizeof(node_t);

  wfree(buff4);
  wfree(buff3);
  wfree(buff2);
  wfree(buff);

  print_header(header);

  //check to make sure the above seq. ends with
  //a single free node.
  assert(header->size == page_size - sizeof(node_t));
  assert(header->is_free == 1);
  assert(header->bwd == NULL);
  assert(header->fwd == NULL);

  destroy();
}

int main() {
  test_init_destroy();
  test_allocation_basic();
  test_free_basic();
  test_allocation_withsplits();
  test_allocationfree_placement();
  test_free_coalescing_case1();
  test_free_coalescing_case2();
  test_free_coalescing_case3();
  //test_free_coalescing_chains_fwd();
  test_free_coalescing_chains_bwd();

  printf("All tests passed!\n");
}
