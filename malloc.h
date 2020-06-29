#ifndef _MALOC_H_
#define _MALOC_H_

#include "custom_unistd.h"
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 4096
#define fence_size 8 //Size of fence in bytes
#define metadata_size (sizeof(struct chunk_t) + fence_size * 2)
#define move_to_data_block (sizeof(struct chunk_t) + fence_size)
#define next_block(last_block) (((char *)last_block) + metadata_size + last_block -> size)
#define prev_block(block) (((char *)block) - (metadata_size + block -> prev -> size))


#define heap_malloc(bytes) heap_malloc_debug(bytes, __LINE__, __FILE__)
#define heap_calloc(n, size_of_element) heap_calloc_debug(n, size_of_element, __LINE__, __FILE__)
#define heap_realloc(ptr, new_size) heap_realloc_debug(ptr, new_size, __LINE__, __FILE__)
#define heap_malloc_aligned(bytes) heap_malloc_aligned_debug(bytes, __LINE__, __FILE__)
#define heap_calloc_aligned(n, size_of_element) heap_calloc_aligned_debug(n, size_of_element, __LINE__, __FILE__)
#define heap_realloc_aligned(ptr, new_size) heap_realloc_aligned_debug(ptr, new_size, __LINE__, __FILE__)



enum pointer_type_t
{
    pointer_null,
    pointer_out_of_heap,
    pointer_control_block,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

struct chunk_t
{
    struct chunk_t * next;
    struct chunk_t * prev;
    size_t size;
    int taken_flag; //1 - in use | 0 - empty
    int checksum;
    int line;
    const char * filename;
};

typedef struct heap_t
{
    void * heap;
    int checksum;
    size_t max_heap_size;
    struct chunk_t * first_chunk;
    size_t chunk_count;
} heap;

uint32_t add_bytes(void * ptr, uint32_t data_size);
struct chunk_t * find_suitable_block(uint32_t needed_space);
struct chunk_t * heap_get_last_block();
size_t page_size(size_t number);
int coalesce_blocks(struct chunk_t * temp);
void split(struct chunk_t * chunk, size_t bytes);
size_t get_payload_size(void * ptr);

void destroy_mutex(void);
int heap_reset(void);
int heap_validate(void);
int heap_setup(void);

void heap_free(void *);
void heap_dump_debug_information(void);

void * heap_malloc_debug(size_t, int, const char *);
void * heap_calloc_debug(size_t, size_t, int, const char *);
void * heap_realloc_debug(void *, size_t, int, const char *);

void * heap_malloc_aligned_debug(size_t, int, const char *);
void * heap_calloc_aligned_debug(size_t, size_t, int, const char *);
void * heap_realloc_aligned_debug(void *, size_t, int, const char *);

void * heap_get_data_block_start(const void * pointer);

size_t heap_get_used_space(void);
size_t heap_get_largest_used_block_size(void);
size_t heap_get_free_space(void);
size_t heap_get_largest_free_area(void);
size_t heap_get_block_size(const const void * memblock);

uint64_t heap_get_used_blocks_count(void);
uint64_t heap_get_free_gaps_count(void);

enum pointer_type_t get_pointer_type(const const void * pointer);

#endif


