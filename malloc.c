#include <unistd.h>   // getpagesize
#include <inttypes.h> // uint32_t
#include <sys/mman.h> // mman
#include <pthread.h>  // mutex


// Del in release
#include <string.h>   // memcpy
#include <stdio.h>  // printf
#include <stdlib.h> // orig malloc

#define True 1
#define False 0

#define MEMORY_IS_BUSY 0x03

// TODO Memory alignment
typedef struct s_chunk
{
    uint64_t        size;
    struct s_chunk* prev;
    struct s_chunk* next;
    _Bool           in_use;

}   chunk_t;


pthread_mutex_t g_malloc_mutex = PTHREAD_MUTEX_INITIALIZER;


// ret_chunk will be busy, memory that remains go to current_chunk
void procces_new_chunk (chunk_t* current_chunk, chunk_t* ret_chunk, size_t sizemem)
{
    uint64_t new_size;
    chunk_t* new_next;

    new_size = current_chunk->size - sizemem - sizeof(chunk_t);
    new_next = current_chunk->next;

    ret_chunk = current_chunk;
    ret_chunk->size = sizemem;
    ret_chunk->prev = current_chunk->prev;
    ret_chunk->next = current_chunk;
    ret_chunk->in_use = True;

    current_chunk = ret_chunk + sizemem + sizeof(chunk_t);
    current_chunk->size = new_size;
    current_chunk->prev = ret_chunk;
    current_chunk->next = new_next;
    current_chunk->in_use = True;

//  was: chunk->current_chunk->chunk
//  now: chunk->ret_chunk->current_chunk->chunk
}


int search_available_memory(chunk_t* chunks_list_head, chunk_t* ret_chunk, size_t sizemem)
{
    chunk_t* current_chunk;

    current_chunk = chunks_list_head;

    while (current_chunk != NULL) {

        if ((current_chunk->in_use != True) && (current_chunk->size >= (sizemem + sizeof(chunk_t)))) {
            // Found the right place
            procces_new_chunk(current_chunk, ret_chunk, sizemem);
            return 0;
        }

        current_chunk = current_chunk->next;
    }

    return MEMORY_IS_BUSY;
}


int defragmentation_alloc_memory(chunk_t* chunks_list, chunk_t* ret_chunk, size_t sizemem)
{

}


void find_chunk_tail(chunk_t* chunks_list_head, chunk_t* ret_chunks_tail)
{
    static chunk_t* chunk_tail;

    if (chunk_tail == NULL) {
        chunk_tail = chunks_list_head;
    }

    while (chunk_tail->next != NULL) {
        chunk_tail = chunk_tail->next;
    }
    ret_chunks_tail = chunk_tail;
}

void init_new_chunk(chunk_t* new_chunk, size_t size, chunk_t* prev_chunk)
{
    new_chunk->size = size;
    new_chunk->prev = prev_chunk;
    new_chunk->in_use = False;
}

void req_new_page(chunk_t* chunks_list_head, chunk_t* ret_chunk, size_t sizemem)
{
    chunk_t* chunks_tail;
    void* addr;

    chunks_tail = chunks_list_head;
    // TODO Can create global tail if iterate too long
    while (chunks_tail->next != NULL) {
        chunks_tail = chunks_tail->next;
    }

    // Why mmap not failed with 100TB
    // TODO IF I ASK 1MB RETURN 1PAGE! HANDLE IT!
    addr = mmap(NULL, sizemem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

    if (addr == MAP_FAILED ) {
        ret_chunk = NULL;
    }
    else {
        ret_chunk = addr;
        ret_chunk->size = sizemem;
        ret_chunk->prev = chunks_tail;
        ret_chunk->next = NULL;
        ret_chunk->in_use = False;
    }
}


static void* ft_malloc(size_t sizemem)
{
    // Search in "my" memory
    // If "my" memory is not enough - defragmentation
    // If memory is still insufficient - request memory from the system
    int retcode;
    chunk_t* ret_chunk;

    pthread_mutex_lock(&g_malloc_mutex);

    static chunk_t chunks_list;

    retcode = -1;
    ret_chunk = NULL;

    // Search in already allocated memory
    retcode = search_available_memory(&chunks_list, ret_chunk, sizemem);

    // Defragmentation allocated memory
    // TODO go in free() del in malloc()
    if (retcode == MEMORY_IS_BUSY) {
        retcode = defragmentation_alloc_memory(&chunks_list, ret_chunk, sizemem);
    }

    // New page
    if (retcode == MEMORY_IS_BUSY) {
        req_new_page(&chunks_list, ret_chunk, sizemem);
    }

    pthread_mutex_unlock(&g_malloc_mutex);

    return (void*)(ret_chunk + sizeof(chunk_t));
}

int main() {

    size_t page_size = getpagesize();
    printf("Page size: %lu\n", page_size);

    char *buffer = (char *) ft_malloc(10);

    if (buffer == NULL) {
        printf("Can not locate memmory\n");
        return 0;
    }
}

//#include <time.h>
//int main() {
//    char *buffer = (char *) ft_malloc(20000000000000);
//    // How check alloc memory
//
//    if (buffer == NULL) {
//        printf("Can not locate memmory\n");
//        return 0;
//    }
//
//    time_t start;
//    time_t end;
//
//    start = time(NULL);
//    for (size_t i = 0; i < 50000000000; ++i)
//    {
//        buffer[i] = 'a';
//    }
//    end = time(NULL);
////    buffer[10000000000] = 'a';
//    printf("memsize: %c\n", buffer[50000000000 - 1]);
//    printf("time: %ld\n", end-start);
//
//    return 0;
//}

// glibc malloc allocate 1-12.5MB

// Если мне нужно памяти больше чем page_size. mmap может выделить страницы не попорядку?
// Если да - они не упадут в один chunk и будет необходим исключительный механиз обработки таких запросов от malloc