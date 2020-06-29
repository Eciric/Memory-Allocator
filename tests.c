#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include "malloc.h"


int main(int argc, char **argv)
{
    //####################################################################
    //                            HEAP_SETUP
    
        int status = heap_setup();
        assert(status == 0);
        
    //####################################################################

    //####################################################################
    //                              MALLOC
    
        void * testM = heap_malloc(-1); //Should return NULL
        assert(testM == NULL);

        testM = heap_malloc(0); // Should return NULL
        assert(testM == NULL);

        testM = heap_malloc(4); //Shouldn't return NULL
        assert(testM != NULL);    

        assert(get_payload_size(testM) == 4); //Sizes should match
    
    //####################################################################
    
    heap_reset();

    //####################################################################
    //                              CALLOC
    
        char * testC = heap_calloc(-1, 0); //Should return NULL
        assert(testC == NULL);

        testC = heap_calloc(0, 0); // Should return NULL
        assert(testC == NULL);

        testC = heap_calloc(10, 1); //Shouldn't return NULL
        assert(testC != NULL);    
        assert(get_payload_size(testC) == 10); //Sizes should match

        for (int i = 0; i < 10; i++)
        {
            assert(testC[i] == 0);
        }

    //####################################################################

    heap_reset();

    //####################################################################
    //                              REALLOC
    
        int * testR = heap_realloc(NULL, -1); //Should return NULL
        assert(testR == NULL);

        testR = heap_realloc(NULL, 4); //Should work like a malloc
        assert(testR != NULL);    

        assert(get_payload_size(testR) == 4); //Sizes should match
        
        testR = heap_realloc(testR, 0); // Should work like a heap_free
        assert(get_pointer_type(testR) == pointer_unallocated);

        testR = heap_realloc(NULL, 4); 
        assert(testR != NULL);

        *testR = 25;

        int * testR2 = heap_realloc(testR, 16); //Should allocate new block
        assert(testR2 != NULL);
        assert(get_payload_size(testR2) == 16);
        assert(*(testR2) == 25);

    //####################################################################

    heap_reset();

    //####################################################################
    //                              FREE

        void * testF = heap_malloc(4);
        assert(testF != NULL);
        heap_free(testF);
        assert(get_pointer_type(testF) == pointer_unallocated);

    //####################################################################

    heap_reset();

    //####################################################################
    //                         MALLOC_ALIGNED

        int * testMA = heap_malloc_aligned(4);
        assert(testMA != NULL);
        assert(get_pointer_type(testMA) == pointer_valid);
        assert((intptr_t)testMA % PAGE_SIZE == 0);

        heap_free(testMA);
        assert(get_pointer_type(testMA) == pointer_inside_data_block);

    //####################################################################

    heap_reset();

    //####################################################################
    //                         CALLOC_ALIGNED

        char * testCA = heap_calloc_aligned(10, 1);
        assert(testCA != NULL);
        assert(get_pointer_type(testCA) == pointer_valid);
        assert((intptr_t)testCA % PAGE_SIZE == 0);
        
        for (int i = 0; i < 10; i++)
        {
            assert(testCA[i] == 0);
        }
    
    //####################################################################

    heap_reset();

    //####################################################################
    //                         REALLOC_ALIGNED

        int * testRA = heap_realloc_aligned(NULL, -1); //Should return NULL
        assert(testRA == NULL);

        testRA = heap_realloc_aligned(NULL, 4); //Should work like a malloc
        assert(get_pointer_type(testRA) == pointer_valid);
        assert((intptr_t)testRA % PAGE_SIZE == 0);

        assert(get_payload_size(testRA) == 4); //Sizes should match
        
        testRA = heap_realloc_aligned(testRA, 0); // Should work like a heap_free
        assert(get_pointer_type(testRA) == pointer_inside_data_block);

        testRA = heap_realloc_aligned(NULL, 4); 
        assert(get_pointer_type(testRA) == pointer_valid);
        assert((intptr_t)testRA % PAGE_SIZE == 0);

        int * testRA2 = heap_realloc_aligned(testRA, 16); //Shouldn't allocate new block
        assert(get_pointer_type(testRA2) == pointer_null);

    //####################################################################

    heap_reset();

    //####################################################################
    //                          HEAP_VALIDATE

        //--------------------------------------------------
        //Basic heap integrity checks that should be valid
        const int VALID_HEAP_STATUS = 0;
        assert(heap_validate() == VALID_HEAP_STATUS);

        void * testV = heap_malloc(4);
        assert(heap_validate() == VALID_HEAP_STATUS);

        heap_free(testV);
        assert(heap_validate() == VALID_HEAP_STATUS);

        for (int i = 0; i < 10; i++)
        {
            heap_malloc(10 + i);
        }

        assert(heap_validate() == VALID_HEAP_STATUS);
        heap_reset();

        void * testV2 = heap_malloc(16);
        void * testV2_1 = heap_malloc(16);
        assert(heap_validate() == VALID_HEAP_STATUS);
        //--------------------------------------------------

        // //Create pointers for breaching heap integrity
        struct chunk_t * block = ((struct chunk_t *)((char *)testV2 - move_to_data_block));
        struct chunk_t * block2 = block -> next;
        struct chunk_t * next = ((struct chunk_t *)((char *)testV2 - move_to_data_block)) -> next;
        struct chunk_t * prev = ((struct chunk_t *)((char *)testV2 - move_to_data_block)) -> prev;
        struct chunk_t * next2 = ((struct chunk_t *)((char *)testV2_1 - move_to_data_block)) -> next;
        struct chunk_t * prev2 = ((struct chunk_t *)((char *)testV2_1 - move_to_data_block)) -> prev;

        //Corrupting pointers
        block -> next = NULL; //Assign wrong pointer
        assert(heap_validate() < 0);
        block -> next = next; //Restore heap integrity

        block -> prev = next; //Assign wrong pointer
        assert(heap_validate() < 0);
        block -> prev = prev; //Restore heap integrity

        block2 -> next = next; //Assign wrong pointer
        assert(heap_validate() < 0); 
        block2 -> next = next2; //Restore heap integrity 

        block2 -> prev = NULL; //Assign wrong pointer
        assert(heap_validate() < 0);
        block2 -> prev = prev2; //Restore heap integrity

        //Corrupting checksums
        block -> checksum += 1; 
        assert(heap_validate() < 0);
        block -> checksum = 0; 
        block -> checksum = add_bytes(block, sizeof(struct chunk_t));

        block2 -> checksum += 1;
        assert(heap_validate() < 0);
        block2 -> checksum = 0;
        block2 -> checksum = add_bytes(block2, sizeof(struct chunk_t));

        //Corrupting fences
        char fence[fence_size];
        for (int i = 0; i < fence_size; i++) fence[i] = i;

        memset((void *)((char *)block) + sizeof(struct chunk_t), 0, fence_size);
        assert(heap_validate() < 0);
        memcpy((void *)((char *)block) + sizeof(struct chunk_t), fence, fence_size);
        assert(heap_validate() == VALID_HEAP_STATUS);

        memset((void *)((char *)block) + move_to_data_block + block -> size, 0, fence_size);
        assert(heap_validate() < 0);
        memcpy((void *)((char *)block) + move_to_data_block + block -> size, fence, fence_size);
        assert(heap_validate() == VALID_HEAP_STATUS);

        memset((void *)((char *)block2) + sizeof(struct chunk_t), 0, fence_size);
        assert(heap_validate() < 0);
        memcpy((void *)((char *)block2) + sizeof(struct chunk_t), fence, fence_size);
        assert(heap_validate() == VALID_HEAP_STATUS);

        memset((void *)((char *)block2) + move_to_data_block + block -> size, 0, fence_size);
        assert(heap_validate() < 0);
        memcpy((void *)((char *)block2) + move_to_data_block + block -> size, fence, fence_size);
        assert(heap_validate() == VALID_HEAP_STATUS);

    //####################################################################

    heap_reset();

    //####################################################################
    //                   HEAP_GET_DATA_BLOCK_START       

        void * testHGDBS = heap_malloc(10);

        void * start = (void *)((char *)testHGDBS - move_to_data_block);

        assert(heap_get_data_block_start(testHGDBS) == start);

    //####################################################################

    heap_reset();

    //####################################################################
    //                        HEAP_GET_USED_SPACE       

        size_t space = metadata_size;
        assert(heap_get_used_space() == space);


        void * testHGUS = heap_malloc(4);
        heap_free(testHGUS);

        assert(heap_get_used_space() == space);

    //####################################################################

    heap_reset();

    //####################################################################
    //                  HEAP_GET_LARGEST_USED_BLOCK_SIZE

        void * testHGLUBS = heap_malloc(10);
        void * testHGLUBS2 = heap_malloc(30);
        void * testHGLUBS3 = heap_malloc(20);

        assert(heap_get_largest_used_block_size() == 30);

    //####################################################################

    heap_reset();

    //####################################################################
    //                        HEAP_GET_FREE_SPACE       

        int free_space = PAGE_SIZE * 2 - metadata_size;

        assert(heap_get_free_space() == free_space);

    //####################################################################
    
    heap_reset();
    
    //####################################################################
    //                   HEAP_GET_LARGEST_FREE_AREA       

        void * testHGLFA = heap_malloc(5000);
        void * testHGLFA2 = heap_malloc(1000);
        heap_free(testHGLFA);

        assert(heap_get_largest_free_area() == 5000);

    //####################################################################

    heap_reset();

    //####################################################################
    //                      HEAP_GET_BLOCK_SIZE       

        void * testHGBS = heap_malloc(123);

        assert(heap_get_block_size(testHGBS) == 123);

    //####################################################################

    heap_reset();

    //####################################################################
    //                     HEAP_GET_USED_BLOCKS_COUNT       

        for (int i = 0; i < 10; i++)
        {
            heap_malloc(10 + i);
        }

        assert(heap_get_used_blocks_count() == 10);

    //####################################################################

    heap_reset();

    //####################################################################
    //                     HEAP_GET_FREE_GAPS_COUNT       


        void * a = heap_malloc(10);
        void * b = heap_malloc(120);
        void * c = heap_malloc(14);
        void * d = heap_malloc(104);
        void * e = heap_malloc(18);
        heap_free(b);
        heap_free(d);

        //Assert == 3 because of last block holding the remaining heap mem
        assert(heap_get_free_gaps_count() == 3);

    //####################################################################

    heap_reset();

    //####################################################################
    //                          GET_POINTER_TYPE

        void * testGPT = heap_malloc(10);
        assert(get_pointer_type(testGPT) == pointer_valid);
        assert(get_pointer_type((void *)((char *)testGPT - move_to_data_block)) == pointer_control_block);
        assert(get_pointer_type((void *)((char *)testGPT - move_to_data_block - 1)) == pointer_out_of_heap);
        assert(get_pointer_type((void *)((char *)testGPT + PAGE_SIZE * 3)) == pointer_out_of_heap);
        assert(get_pointer_type(NULL) == pointer_null);
        heap_free(testGPT);
        assert(get_pointer_type(testGPT) == pointer_unallocated);
        assert(get_pointer_type((void *)((char *)testGPT + 1)) == pointer_inside_data_block);

    //####################################################################

    heap_reset();

    //####################################################################
    //                          DEFAULT_TEST

        // parametry pustej sterty
        size_t free_bytes = heap_get_free_space();
        size_t used_bytes = heap_get_used_space();

        void* p1 = heap_malloc(8 * 1024 * 1024); // 8MB
        void* p2 = heap_malloc(8 * 1024 * 1024); // 8MB
        void* p3 = heap_malloc(8 * 1024 * 1024); // 8MB
        void* p4 = heap_malloc(45 * 1024 * 1024); // 45MB
        assert(p1 != NULL); // malloc musi się udać
        assert(p2 != NULL); // malloc musi się udać
        assert(p3 != NULL); // malloc musi się udać
        assert(p4 == NULL); // nie ma prawa zadziałać
        // Ostatnia alokacja, na 45MB nie może się powieść,
        // ponieważ sterta nie może być aż tak 
        // wielka (brak pamięci w systemie operacyjnym).


        status = heap_validate();
        assert(status == 0); // sterta nie może być uszkodzona

        // zaalokowano 3 bloki
        assert(heap_get_used_blocks_count() == 3);

        // zajęto 24MB sterty; te 2000 bajtów powinno
        // wystarczyć na wewnętrzne struktury sterty
        assert(
            heap_get_used_space() >= 24 * 1024 * 1024 &&
            heap_get_used_space() <= 24 * 1024 * 1024 + 2000
            );

        // zwolnij pamięć
        heap_free(p1);
        heap_free(p2);
        heap_free(p3);

        // wszystko powinno wrócić do normy
        assert(heap_get_free_space() == free_bytes);
        assert(heap_get_used_space() == used_bytes);

        // już nie ma bloków
        assert(heap_get_used_blocks_count() == 0);

    //####################################################################
    
    //Clean up
	destroy_mutex();
	return 0;
}

