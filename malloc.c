#include "malloc.h"
#include <pthread.h>

heap myHeap;
struct chunk_t firstChunk;
pthread_mutex_t myMutex = PTHREAD_MUTEX_INITIALIZER;

void destroy_mutex()
{
    pthread_mutex_destroy(&myMutex);
}

int heap_validate(void)
{
    //Returns:
    //-1 : Heap struct is wrong
    //-2 : First chunk of heap is wrong
    //-3 : Other chunks of heap are wrong;
    
    //Validate heap itself (pointers pointing correctly and checksums are valid)
    if (myHeap.heap == NULL) return -1;
    
    int tempChecksum = myHeap.checksum;
    myHeap.checksum = 0;
    if (tempChecksum != add_bytes(&myHeap, sizeof(myHeap))) return -1;
    myHeap.checksum = tempChecksum;

    if (myHeap.max_heap_size < PAGE_SIZE) return -1;
    if (myHeap.chunk_count < 0) return -1;

    //Validate first chunk (pointers pointing correctly and checksums are valid)
    if (myHeap.first_chunk != myHeap.heap) {printf("First block address isn't heap address\n"); return -2;}
    if (myHeap.first_chunk -> next == NULL && myHeap.chunk_count > 1) {printf("First block next pointer is NULL\n"); return -2;}
    if (myHeap.first_chunk -> prev != NULL) {printf("First block prev pointer isn't NULL\n"); return -2;}
    
    tempChecksum = myHeap.first_chunk -> checksum;
    myHeap.first_chunk -> checksum = 0;
    if (tempChecksum != add_bytes(myHeap.first_chunk, sizeof(struct chunk_t))) {printf("First block checksum is incorrect\n"); return -2;}
    myHeap.first_chunk -> checksum = tempChecksum;    
    

    
    if (myHeap.chunk_count > 0)
    {
        char fence[fence_size];
        for (int i = 0; i < fence_size; i++)
        {
            fence[i] = i;
        }
        //Validate first chunk fences
        char * chunk_fence = (((char *)myHeap.first_chunk) + sizeof(struct chunk_t));
        char * chunk_fence2 = (((char *)myHeap.first_chunk) + move_to_data_block + myHeap.first_chunk -> size);
        for (int i = 0; i < fence_size; i++)
        {
            if (fence[i] != *(chunk_fence + i)) {printf("First block left fence is incorrect\n"); return -2;}
            if (fence[i] != *(chunk_fence2 + i)) {printf("First block right fence is incorrect\n"); return -2;}
        }

        //Validate next chunks and their fences
        struct chunk_t * temp = myHeap.first_chunk -> next;
        for (int i = 1; i < myHeap.chunk_count; i++)
        {
            //Pointer check - shouldn't be NULL
            if (temp == NULL) 
            {
                printf("Block %d is null\n", i);
                return -3;
            }

            //Metadata of chunks
            if (temp -> size < 0) {printf("Block of ID: %d size is negative\n", i); return -3;}
            if (temp -> taken_flag < 0 || temp -> taken_flag > 1) {printf("Taken flags of block: %d are incorrect\n", i); return -3;}
            if (temp -> prev == NULL) {printf("Block of ID: %d prev pointer is NULL\n", i); return -3;}
            if (temp -> next == NULL && (i != myHeap.chunk_count-1)) {printf("Block of ID: %d next pointer is NULL\n", i); return -3;}
            if (temp -> next != (struct chunk_t *)next_block(temp)) 
            {
                printf("Block next pointer is incorrect\n"); 
                printf("Block of size: %lu and ID: %d\n", temp -> size, i);
                printf("Temp -> next: %p\n", temp -> next);
                printf("Temp -> next should be: %p\n", (struct chunk_t *)next_block(temp));
                return -3;
            }
            
            if (temp -> prev != (struct chunk_t *)prev_block(temp)) 
            {
                printf("Block prev pointer is incorrect\n"); 
                printf("Block of size: %lu and ID: %d\n", temp -> size, i);
                printf("Temp -> prev: %p\n", temp -> prev);
                printf("Temp -> prev should be: %p\n", (struct chunk_t *)prev_block(temp));
                return -3;
            }
            if (temp -> line < 0) {printf("Block line is negative\n"); return -3;}
            if (temp -> filename == NULL) {printf("Block filename is NULL\n"); return -3;}

            tempChecksum = temp -> checksum;
            temp -> checksum = 0;
            if (tempChecksum != add_bytes(temp, sizeof(struct chunk_t))) {printf("Block checksum is incorrect\n"); return -3;}
            temp -> checksum = tempChecksum;
            

            //Fences of chunks
            chunk_fence = (((char *)temp) + sizeof(struct chunk_t));
            chunk_fence2 = (((char *)temp) + move_to_data_block + temp -> size);
            
            for (int i = 0; i < fence_size; i++)
            {
                if (fence[i] != *(chunk_fence + i)) {printf("Block left fence is incorrect\n"); return -3;}
                if (fence[i] != *(chunk_fence2 + i)) {printf("Block right fence is incorrect\n"); return -3;}
            }

            temp = temp -> next;
        }

    }
    return 0;
}

int heap_reset(void)
{
    if (heap_validate() < 0)
    {
        printf("Heap_reset detected heap integrity breach\n");
        return -1;
    }

    void * res = custom_sbrk(-myHeap.max_heap_size);
    if (res == ((void *)-1)) 
    {
        printf("Heap reset failed at resetting the heap\n");
        return -1;
    }
    if (heap_setup() < 0) return -1;

    return 0;
}

int heap_setup(void)
{
    //Init firstChunk
    firstChunk.prev = NULL;
    firstChunk.next = NULL;
    firstChunk.size = PAGE_SIZE * 2 - metadata_size;
    firstChunk.taken_flag = 0;
    firstChunk.line = __LINE__;
    firstChunk.filename = __FILE__;
    firstChunk.checksum = 0;
    firstChunk.checksum = add_bytes(&firstChunk, sizeof(firstChunk));
    //Init myHeap
    myHeap.max_heap_size = PAGE_SIZE * 2;
    myHeap.heap = custom_sbrk(PAGE_SIZE * 2);
    if (myHeap.heap == ((void *)-1))
    {
        printf("Heap setup failed at requesting initial memory from OS\n");
        return -1;
    }

    myHeap.chunk_count = 0;
    myHeap.first_chunk = myHeap.heap;
    
    myHeap.checksum = 0;
    myHeap.checksum = add_bytes(&myHeap, sizeof(myHeap));
    memcpy(myHeap.heap, &firstChunk, sizeof(firstChunk));

    //Check for heap integrity
    int res = 0;
    if ((res = heap_validate()) < 0)
    {
        printf("Heap setup failed at assuring heap integrity: %d\n", res);
        return -1;
    }

    return 0;
}

uint32_t add_bytes(void * ptr, uint32_t data_size)
{
    if (!ptr || data_size < 1) return 0;

    uint32_t sum = 0;
    for (uint32_t i = 0; i < data_size; i++)
    {
        sum += (*(((unsigned char *)ptr) + i));
    }

    return sum;
}

struct chunk_t * find_suitable_block(uint32_t needed_space)
{
    struct chunk_t * temp = myHeap.first_chunk;

    //First look for suitable freed blocks in the heap
    int suitable_block_found = 0;
    while (temp)
    {
        //This block is perfect and no need to split
        if (temp -> size == needed_space && !(temp -> taken_flag))
        {
            suitable_block_found = 1;
            break;
        }

        //This block can be used but should be splitted
        if ((temp -> size >= (needed_space + metadata_size)) && (temp -> taken_flag == 0))
        {
            split(temp, needed_space);

            suitable_block_found = 1;
            break;
        }
        temp = temp -> next;
    }

    if (!suitable_block_found) temp = NULL;
    return temp;
}

struct chunk_t * heap_get_last_block()
{
    struct chunk_t * last_block = myHeap.first_chunk;
    while (last_block)
    {
        if (last_block -> next == NULL) break;
        last_block = last_block -> next; 
    }
    return last_block;
}

size_t page_size(size_t number)
{
    size_t multiple = PAGE_SIZE;
    return ((number + multiple - 1) / multiple) * multiple;
}

int coalesce_blocks(struct chunk_t * temp)
{
    if (!temp) return 0;
    
    struct chunk_t * right = temp -> next;

    if (get_pointer_type(right) == pointer_control_block)
    {
        if (right -> taken_flag == 0)
        {
            //Time to coalesce
            if (right -> next)
            {
                right -> next -> prev = temp;
                right -> next -> checksum = 0;
                right -> next -> checksum = add_bytes(right -> next, sizeof(struct chunk_t));
            }
            temp -> next = right -> next;
            temp -> size += (right -> size + metadata_size);
            temp -> checksum = 0;
            temp -> checksum = add_bytes(temp, sizeof(struct chunk_t));

            myHeap.chunk_count--;
            myHeap.checksum = 0;
            myHeap.checksum = add_bytes(&myHeap, sizeof(myHeap));
        }
    }
    else printf("Coalesce blocks didnt get pointer_control_block\n");
    return 1;
}

void split(struct chunk_t * temp, size_t bytes)
{
    struct chunk_t * right = temp -> next;

    //calculate new size for the new block
    int size_of_new_block = temp -> size - bytes - metadata_size;
    //update size of the fitting block
    temp -> size = bytes;
    temp -> checksum = 0;
    temp -> checksum = add_bytes(temp, sizeof(struct chunk_t));
    //memcpy fence at the end of fitting block
    char fence[fence_size];
    for (int i = 0; i < fence_size; i++)
    {
        fence[i] = i;
    }

    memcpy(((char *)temp) + sizeof(struct chunk_t), fence, sizeof(fence));
    memcpy(((char *)temp) + temp -> size + move_to_data_block, fence, sizeof(fence));
    //create newblock with the remaining size
    struct chunk_t newBlock;

    newBlock.prev = temp;
    newBlock.next = right;
    newBlock.size = size_of_new_block;
    newBlock.taken_flag = 0;
    newBlock.line = __LINE__;
    newBlock.filename = __FILE__;
    newBlock.checksum = 0;
    newBlock.checksum = add_bytes(&newBlock, sizeof(struct chunk_t));
    
    myHeap.chunk_count++;
    myHeap.checksum = 0;
    myHeap.checksum = add_bytes(&myHeap, sizeof(myHeap));

    temp -> next = (struct chunk_t *)next_block(temp);
    temp -> next -> checksum = 0;
    temp -> next -> checksum = add_bytes(temp -> next, sizeof(struct chunk_t));
    if (right) right -> prev = temp -> next;
    memcpy(temp -> next, &newBlock, sizeof(newBlock));

    //set left fence of newblock as right doesnt need to be changed
    memcpy(((char *)temp -> next + sizeof(struct chunk_t)), fence, sizeof(fence));

    //update checksum of all blocks changed or created
    temp -> checksum = 0;
    temp -> checksum = add_bytes(temp, sizeof(struct chunk_t));

    if (right)
    {
        right -> checksum = 0;
        right -> checksum = add_bytes(right, sizeof(struct chunk_t));
    }
}

size_t get_payload_size(void * ptr)
{
    if (get_pointer_type(ptr) == pointer_valid)
    {
        struct chunk_t * block = heap_get_data_block_start(ptr);
        return block -> size;
    }

    return 0;
}

void heap_free(void * ptr)
{
    pthread_mutex_lock(&myMutex);
    if (get_pointer_type(ptr) == pointer_valid)
    {
        struct chunk_t * temp = (struct chunk_t *)(((char *)ptr) - (move_to_data_block));
        if (get_pointer_type(temp) != pointer_control_block)
        {
            printf("Invalid pointer passed to heap_free\n");
        }
        temp -> taken_flag = 0;
        temp -> checksum = 0;
        temp -> checksum = add_bytes(temp, sizeof(struct chunk_t));

        //Coalesce free blocks if such exist next to each other
        if (temp -> prev && temp -> prev -> taken_flag == 0)
        {
            temp = temp -> prev;
            coalesce_blocks(temp);
        }

        if (temp -> next && temp -> next -> taken_flag == 0) 
        {
            coalesce_blocks(temp);
        }

        if (heap_get_used_blocks_count() == 0)
        {
            if (heap_reset() < 0)
            {
                printf("Couldn't reset heap!\n");
            }
        }
    }
    else
    {
        printf("Invalid pointer passed to heap_free!\n");
        printf("Passed pointer: %p\n", ptr);
    }
    pthread_mutex_unlock(&myMutex);
}

void heap_dump_debug_information(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach in heap_dump_debug_information\n");
        return;
    }

    int chunk_counter = 0;
    struct chunk_t * temp = myHeap.first_chunk;

    printf("################################\n");
    printf("HEAP STRUCT INFORMATION DUMP:\n");
    printf("HEAP ADDRESS: %p\n", myHeap.heap);
    printf("HEAP FIRST_CHUNK ADDRESS: %p\n", myHeap.first_chunk);
    printf("HEAP CHECKSUM: %d\n", myHeap.checksum);
    printf("HEAP CURRENT FREE SIZE: %lu\n", heap_get_free_space());
    printf("HEAP MAX SIZE: %lu\n", myHeap.max_heap_size);
    printf("HEAP CHUNKS IN USE: %lu\n", myHeap.chunk_count);
    printf("HEAP MAX ADDRESS: %p\n", (void *)((char *)myHeap.heap + myHeap.max_heap_size));
    printf("HEAP BIGGEST BLOCK: %lu\n", heap_get_largest_used_block_size());
    printf("################################\n");
    
    printf("\n");
    
    printf("################################\n");
    printf("HEAP CHUNKS INFORMATION:\n");

    while (temp)
    {
        printf("----------------------------------------\n");
        printf("CHUNK NUMBER: %d\n", chunk_counter);
        printf("CHUNK ADDRESS: %p\n", temp);
        printf("CHUNK PREV ADDRESS: %p\n", temp -> prev);
        printf("CHUNK NEXT ADDRESS: %p\n", temp -> next);
        printf("CHUNK CHECKSUM: %d\n", temp -> checksum);
        printf("CHUNK PAYLOAD SIZE: %lu\n", temp -> size);
        printf("CHUNK ACTUAL SIZE: %lu\n", temp -> size + metadata_size);
        printf("CHUNK TAKEN FLAG: %d\n", temp -> taken_flag);
        printf("CHUNK ALLOCATED IN LINE: %d\n", temp -> line);
        printf("CHUNK ALLOCATED IN FILE: %s\n", temp -> filename);
        printf("----------------------------------------\n");
        printf("\n");
        temp = temp -> next;
        chunk_counter++;
    }
    
    printf("END OF CHUNKS\n");
    printf("################################\n");
}

void * heap_malloc_debug(size_t bytes, int line, const char * filename)
{
    pthread_mutex_lock(&myMutex);
    //Malloc code here with bonus information about blocks allocated or failures
    if (!bytes) 
    {
        printf("Called malloc with 0 amount of bytes\n");
        printf("Malloc called in line: %d\nAnd filename: %s\n", line, filename);        
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (bytes + sizeof(struct chunk_t) < bytes)
    {
        printf("Called malloc with negative amount of bytes\n");
        printf("Malloc called in line: %d\nAnd filename: %s\n", line, filename);        
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach\n");
        printf("Malloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    //Add a new block of requested size to the heap and return pointer to the data block
    char fence[fence_size];
    for (int i = 0; i < fence_size; i++)
    {
        fence[i] = i;
    }

    //Function returns a freed block of at least required space if such exists
    //It is also responsible for splitting blocks

    struct chunk_t * suitableBlock;
    suitableBlock = find_suitable_block(bytes);
    
    //If NULL was returned we failed to find a suitable block
    //Check if we have enough space to push the block at the "end" of the heap
    //If not ask OS for more memory and put the block there
    if (suitableBlock == NULL && myHeap.chunk_count > 0)
    {
        struct chunk_t * last_block = heap_get_last_block();
        
        if (((char *)myHeap.heap + myHeap.max_heap_size) - ((char *)last_block + last_block -> size) <= (bytes + metadata_size))
        {
            void * res = custom_sbrk(page_size(bytes + metadata_size));
            if (res == ((void *)-1))
            {
                printf("Couldn't request more memory from OS\n");
                printf("Malloc called in line: %d\nAnd filename: %s\n", line, filename);
                pthread_mutex_unlock(&myMutex);
                return NULL;
            }

            myHeap.max_heap_size += page_size(bytes + metadata_size);
            myHeap.checksum = 0;
            myHeap.checksum = add_bytes(&myHeap, sizeof(myHeap));

            //The last block could be taken if it matches perfectly the heap size
            if (last_block -> taken_flag == 1)
            {
                //Create a new free block with the payload of new memory granted by OS - metadata size
                //This should be returned by find_suitable_block later
                firstChunk.filename = __FILE__;
                firstChunk.line = __LINE__;
                firstChunk.next = NULL;
                firstChunk.prev = last_block;
                firstChunk.size = page_size(bytes + metadata_size) - metadata_size;
                firstChunk.taken_flag = 0;
                firstChunk.checksum = 0;
                firstChunk.checksum = add_bytes(&firstChunk, sizeof(struct chunk_t));

                //Update last block structure
                last_block -> next = (struct chunk_t *)next_block(last_block);
                last_block -> checksum = 0;
                last_block -> checksum = add_bytes(last_block, sizeof(struct chunk_t));
                
                //Append the new block at the end of last_block
                memcpy(last_block -> next, &firstChunk, sizeof(struct chunk_t));

                //Append fences
                memcpy(next_block(last_block) + sizeof(struct chunk_t), fence, fence_size);
                memcpy(next_block(last_block) + move_to_data_block + firstChunk.size, fence, fence_size);
            }
            else
            {
                last_block -> size += page_size(bytes + metadata_size);
                last_block -> checksum = 0;
                last_block -> checksum = add_bytes(last_block, sizeof(struct chunk_t));
                //Update the right fence
                memcpy(((char *)last_block + move_to_data_block + last_block -> size), fence, sizeof(fence));
            }
        }

        //It has to find a free block now
        suitableBlock = find_suitable_block(bytes);
        if (suitableBlock == NULL) 
        {
            printf("Something went wrong in MALLOC\n");
            pthread_mutex_unlock(&myMutex);
            return NULL;
        }

        suitableBlock -> size = bytes;
        suitableBlock -> taken_flag = 1;
        suitableBlock -> line = line;
        suitableBlock -> filename = filename;
        suitableBlock -> checksum = 0;
        suitableBlock -> checksum = add_bytes(suitableBlock, sizeof(struct chunk_t));
        
        pthread_mutex_unlock(&myMutex);
        return (((char *)suitableBlock) + move_to_data_block);
    }

    //Suitable block is the myHeap.first_chunk it was partially initialised in the setup function
    //So we don't require the whole malloc algorithm
    if (suitableBlock == myHeap.heap || (suitableBlock == NULL && myHeap.chunk_count == 0))
    {
        //If suitableBlock == NULL then first request for malloc was bigger than initial heap size
        if (suitableBlock == NULL) 
        {
            suitableBlock = myHeap.heap;
            void * res = custom_sbrk(page_size(bytes + metadata_size));
            if (res == ((void *)-1))
            {
                printf("Couldn't request more memory from OS\n");
                printf("Malloc called in line: %d\nAnd filename: %s\n", line, filename);
                pthread_mutex_unlock(&myMutex);
                return NULL;
            }

            myHeap.max_heap_size += page_size(bytes + metadata_size);
            myHeap.checksum = 0;
            myHeap.checksum = add_bytes(&myHeap, sizeof(myHeap));

            suitableBlock -> size += page_size(bytes + metadata_size);
            suitableBlock -> checksum = 0;
            suitableBlock -> checksum = add_bytes(suitableBlock, sizeof(struct chunk_t));
            
            suitableBlock = find_suitable_block(bytes);
        }

        suitableBlock -> size = bytes;
        suitableBlock -> taken_flag = 1;
        suitableBlock -> line = line;
        suitableBlock -> filename = filename;
        suitableBlock -> checksum = 0;
        suitableBlock -> checksum = add_bytes(suitableBlock, sizeof(struct chunk_t));

        memcpy(((char *)suitableBlock) + sizeof(struct chunk_t), fence, sizeof(fence));
        memcpy(((char *)suitableBlock) + sizeof(struct chunk_t) + sizeof(fence) + bytes, fence, sizeof(fence));

        myHeap.chunk_count = 1;        
        myHeap.checksum = 0;
        myHeap.checksum = add_bytes(&myHeap, sizeof(myHeap));

        pthread_mutex_unlock(&myMutex);
        return (((char *)suitableBlock) + move_to_data_block);

    }
    else //Suitable block is a free block
    {
        suitableBlock -> size = bytes;
        suitableBlock -> taken_flag = 1;
        suitableBlock -> line = line;
        suitableBlock -> filename = filename;
        suitableBlock -> checksum = 0;
        suitableBlock -> checksum = add_bytes(suitableBlock, sizeof(struct chunk_t));

        //We now only need to modify the position of the right side fence
        memcpy(((char *)suitableBlock) + suitableBlock->size + metadata_size - fence_size, fence, sizeof(fence));

        pthread_mutex_unlock(&myMutex);
        return (((char *)suitableBlock) + move_to_data_block);
    }
}

void * heap_calloc_debug(size_t n, size_t size_of_element, int line, const char * filename)
{
    pthread_mutex_lock(&myMutex);
    //Calloc code here with bonus information about blocks allocated or failures
    if (heap_validate() < 0)
    {   
        printf("Detected heap integrity breach\n");
        printf("Calloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (n < 1) 
    {
        printf("Calloc given n < 1 elements\n");
        printf("Calloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (size_of_element < 1)
    {
        printf("Calloc given size_of_element < 1\n");
        printf("Calloc called in line:%d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    } 

    pthread_mutex_unlock(&myMutex);
    void * ret = heap_malloc(n * size_of_element);
    pthread_mutex_lock(&myMutex);    
    if (ret != NULL) memset(ret, 0, n * size_of_element);
    pthread_mutex_unlock(&myMutex);
    return ret;
}

void * heap_realloc_debug(void * ptr, size_t new_size, int line, const char * filename)
{
    pthread_mutex_lock(&myMutex);
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach\n");
        printf("Realloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (new_size + sizeof(struct chunk_t) < new_size)
    {
        pthread_mutex_unlock(&myMutex);
        printf("Called realloc with negative bytes!\n");
        printf("Realloc called in line: %d\nAnd filename: %s\n", line, filename);
        return NULL;
    }

    if (!ptr) 
    {
        printf("Called realloc with NULL pointer, executing heap_malloc\n");
        printf("Realloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return heap_malloc(new_size);
    }
    
    if (!new_size) 
    {
        printf("Called realloc with !new_size, executing heap_free\n");
        printf("Realloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        heap_free(ptr);
        return ptr;
    }

    //Try to malloc a block with new_size
    pthread_mutex_unlock(&myMutex);
    void * res = heap_malloc(new_size);
    //If sucessful copy over the contents of old block
    if (res)
    {
        memcpy(res, ptr, new_size);
    }
    else 
    {
        printf("Not enough space on the heap\n");
        printf("Realloc called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    //Free old block
    pthread_mutex_unlock(&myMutex);
    heap_free(ptr);

    //Return the address of new block
    return res;
}

void * heap_malloc_aligned_debug(size_t bytes, int line, const char * filename)
{
    pthread_mutex_lock(&myMutex);
    if (heap_validate < 0)
    {
        printf("Heap_malloc_aligned_debug detected a breach in heap's integrity\n");
        printf("Function called in line: %d in filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (bytes < 1)
    {
        printf("Passed non positive amount of bytes to Heap_malloc_aligned_debug\n");
        printf("Function called in line: %d in filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    //Look for free blocks that lie on the edges of page and check if...
    //They can be splitted to host our new block.

    //For iterating over the heap
    char * temp = (char *)myHeap.heap;

    int steps = myHeap.max_heap_size / PAGE_SIZE - 1;
    if (steps <= 0) 
    {
        pthread_mutex_unlock(&myMutex); 
        return NULL;
    }
    temp += PAGE_SIZE;

    for (int i = 0; i < steps; i++)
    {
        //Checks if we landed in user data and free block
        if (get_pointer_type((void *)temp) == pointer_inside_data_block)
        {
            struct chunk_t * chunk = heap_get_data_block_start(temp);
            if (chunk -> taken_flag == 0)
            {
                //Check if we have to do any splitting
                //If not just return this pointer because the block is perfect
                if (chunk -> size == bytes && (((char *)chunk + move_to_data_block) == temp)) 
                {
                    pthread_mutex_unlock(&myMutex);
                    return (void *)((char *)chunk + move_to_data_block);
                }
                
                // Blocks fits but payload is too large and can be splitted
                if (chunk -> size > (bytes + metadata_size) && (((char *)chunk + move_to_data_block) == temp))
                {
                    split(chunk, bytes);
                    pthread_mutex_unlock(&myMutex);
                    return (void *)((char *)chunk + move_to_data_block);
                }
                
                //Block doesnt fit perfectly, test to see if we can use it
                if ((((char *)chunk + move_to_data_block) != temp))
                {
                    //Get chunks distance from the page end to see if we can fit metadata_size in there
                    int distance_left = (temp - ((char *)chunk + (sizeof(struct chunk_t) + fence_size)));
                    int distance_right = ((char *)chunk + move_to_data_block + chunk -> size) - temp;

                    //This block can be used but we have to check if there can occur a third split
                    if (distance_left > metadata_size && distance_right > (bytes + fence_size + metadata_size)) 
                    {
                        //This means we can split the original block into three blocks
                        split(chunk, distance_left - metadata_size);

                        chunk = chunk -> next;
                        split(chunk, bytes);
                        chunk -> taken_flag = 1;
                        chunk -> line = line;
                        chunk -> filename = filename;
                        chunk -> checksum = 0;
                        chunk -> checksum = add_bytes(chunk, sizeof(struct chunk_t));

                        pthread_mutex_unlock(&myMutex);
                        return (void *)((char *)chunk + move_to_data_block);
                    }
                }                
            }
        }


        temp += PAGE_SIZE;
    }

    pthread_mutex_unlock(&myMutex);
    return NULL;
}

void * heap_calloc_aligned_debug(size_t n, size_t size_of_element, int line, const char * filename)
{
    //Calloc code here with bonus information about blocks allocated or failures
    pthread_mutex_lock(&myMutex);
    if (heap_validate() < 0)
    {   
        printf("Detected heap integrity breach\n");
        printf("Calloc_aligned called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }
    if (n < 1) 
    {
        printf("Calloc_aligned given n < 1 elements\n");
        printf("Calloc_aligned called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }
    if (size_of_element < 1)
    {
        printf("Calloc_aligned given size_of_element < 1\n");
        printf("Calloc_aligned called in line:%d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    } 

    pthread_mutex_unlock(&myMutex);
    void * ret = heap_malloc_aligned(n * size_of_element);
    pthread_mutex_lock(&myMutex);
    if (ret != NULL) memset(ret, 0, n * size_of_element);
    pthread_mutex_unlock(&myMutex);
    return ret;
}

void * heap_realloc_aligned_debug(void * ptr, size_t new_size, int line, const char * filename)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach\n");
        printf("Realloc_aligned called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }
    
    if (new_size + sizeof(struct chunk_t) < new_size) 
    {
        printf("Detected overflow in realloc_aligned\n");
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }

    if (!ptr) 
    {
        printf("NULL passed to realloc_aligned, executing malloc_aligned\n");
        pthread_mutex_unlock(&myMutex);
        return heap_malloc_aligned(new_size);
    }

    if (!new_size) 
    {
        printf("Realloc_aligned given size 0, executing heap_free\n");
        pthread_mutex_unlock(&myMutex);
        heap_free(ptr);
        return ptr;
    }

    //Try to malloc a block with new_size
    pthread_mutex_unlock(&myMutex);
    void * res = heap_malloc_aligned(new_size);
    pthread_mutex_lock(&myMutex);

    //If sucessful copy over the contents of old block
    if (res)
    {
        memcpy(res, ptr, heap_get_block_size(res));
    }
    else 
    {
        printf("Not enough space on the heap\n");
        printf("Realloc_aligned called in line: %d\nAnd filename: %s\n", line, filename);
        pthread_mutex_unlock(&myMutex);
        return NULL;
    }
    
    //Free old block
    pthread_mutex_unlock(&myMutex);
    heap_free(ptr);

    //Return the address of new block
    return res;
}

void * heap_get_data_block_start(const void * pointer)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_data_block_start\n");
        return NULL;
    }

    if (pointer == NULL) return NULL;

    enum pointer_type_t pointer_validation = get_pointer_type(pointer);

    if (pointer_validation != pointer_valid && pointer_validation != pointer_inside_data_block) return NULL;

    struct chunk_t * temp = myHeap.first_chunk;
    while (temp)
    {
        char * temp_data = ((char *)temp) + sizeof(struct chunk_t) + fence_size;
        if ((char *)pointer >= temp_data && (char *)pointer <= temp_data + temp -> size) return temp;
        temp = temp -> next;
    }

    return NULL;
}

size_t heap_get_used_space(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_used_space\n");
        return 0;
    }
    return (myHeap.max_heap_size - heap_get_free_space());
}

size_t heap_get_largest_used_block_size(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_largest_used_block_size\n");
        return 0;
    }

    struct chunk_t * temp = myHeap.first_chunk;

    int largest_found_size = temp -> size;
    while (temp)
    {
        if (temp -> next == NULL) break;
        if (temp -> size > largest_found_size)
        {
            largest_found_size = temp -> size;
        }
        temp = temp -> next;
    }

    return largest_found_size;    
}

size_t heap_get_free_space(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_free_space\n");
        return 0;
    }

    struct chunk_t * temp = myHeap.first_chunk;

    size_t size = 0;
    while (temp)
    {
        if (temp -> taken_flag == 0) size += temp -> size;
        temp = temp -> next;
    }
    return size;
}

size_t heap_get_largest_free_area(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_largest_free_area\n");
        return 0;
    }

    struct chunk_t * temp = myHeap.first_chunk;
    int current_largest_free_area = 0;
    while (temp)
    {
        if (temp -> size > current_largest_free_area && temp ->taken_flag == 0) current_largest_free_area = temp -> size;
        temp = temp -> next;
    }

    return current_largest_free_area;
}

size_t heap_get_block_size(const const void * memblock)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_block_size\n");
        return 0;
    }

    if (get_pointer_type(memblock) != pointer_valid) return 0;
    struct chunk_t * temp = (struct chunk_t *)((char *)memblock - move_to_data_block);
    size_t size = temp -> size;

    return size;
}

uint64_t heap_get_used_blocks_count(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_used_blocks_count\n");
        return 0;
    }

    struct chunk_t * temp = myHeap.first_chunk;
    uint64_t used_blocks = 0;
    while (temp)
    {
        if (temp -> taken_flag) used_blocks++;
        temp = temp -> next;
    }

    return used_blocks;
}

uint64_t heap_get_free_gaps_count(void)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during heap_get_free_gaps_count\n");
        return 0;
    }

    struct chunk_t * temp = myHeap.first_chunk;
    uint64_t free_blocks = 0;
    size_t machine_word_size = 72;
    while (temp)
    {
        if (!(temp -> taken_flag) && temp -> size >= 72) free_blocks++;
        temp = temp -> next;
    }

    return free_blocks;
}

enum pointer_type_t get_pointer_type(const const void * pointer)
{
    if (heap_validate() < 0)
    {
        printf("Detected heap integrity breach during get_pointer_type\n");
        return pointer_null;
    }

    if (!pointer) return pointer_null;

    //Validate pointer out of heap
    if (pointer < myHeap.heap || pointer > (void *)(((char *)myHeap.heap) + myHeap.max_heap_size))
    {
        return pointer_out_of_heap;
    }

    //Validate pointer_valid and unallocated
    struct chunk_t * temp = myHeap.first_chunk;
    while (temp)
    {
        if (pointer == (void *)(((char *)temp) + move_to_data_block))
        {
            return temp -> taken_flag ? pointer_valid : pointer_unallocated;
        }
        temp = temp -> next;
    }

    //Validate pointer inside data block
    temp = myHeap.first_chunk;
    while (temp)
    {
        if (((char *)pointer >= (((char *)temp) + move_to_data_block)) && ((char *)pointer <= (((char *)temp) + move_to_data_block + temp -> size)))
        {
            return pointer_inside_data_block; 
        }
        temp = temp -> next;
    }

    //Validate pointer control block
    temp = myHeap.first_chunk;
    while (temp)
    {
        if (((char *)pointer >= ((char *)temp)) && (char *)pointer < (((char *)temp) + move_to_data_block)) return pointer_control_block;
        temp = temp -> next;
    }

    return pointer_null;
}