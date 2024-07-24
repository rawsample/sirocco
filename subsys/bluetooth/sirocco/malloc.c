/*
 *  A custom allocator which allocate memory in a pool divided of egual size blocks.
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/sirocco.h>


LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);


/* An interface to control the memory allocation for metric items
 * placed in the FIFOs.
 */

struct memory_block {
    //struct memory_block *next;
    void *data;
    void *next;
};

struct srcc_alloc {
    struct memory_block *free_list;
    //struct memory_block *in_use_list;
    void *memory_pool;
    uint32_t capacity;
    uint32_t count;
    size_t item_size;
};

static struct srcc_alloc conn_allocator = { NULL, NULL, 0, 0, 0 };
static struct srcc_alloc scan_allocator = { NULL, NULL, 0, 0, 0 };
static struct srcc_alloc adv_allocator = { NULL, NULL, 0, 0, 0 };



__attribute__((unused))
static void logdbg_free_list(struct srcc_alloc *allocator)
{
    struct memory_block *mb;
    uint32_t count;
    char buffer[512];

    mb = allocator->free_list;
    count = 0;

    snprintf(buffer, sizeof(buffer), "Free list: ");
    while (mb != NULL) {
        count++;
        snprintf(buffer, sizeof(buffer) - strlen(buffer), "%p (%p) --> ",
                 mb, mb->data);
        mb = mb->next;
    }
    snprintf(buffer, sizeof(buffer) - strlen(buffer), "NULL (count = %d)",
             count);
    LOG_DBG("%s", buffer);
}

static int init_malloc_item(struct srcc_alloc *allocator,
                            uint32_t item_count, size_t item_size)
{
    size_t pool_size;
    size_t block_size;
    struct memory_block *mb;

    block_size = item_size + sizeof(struct memory_block);
    pool_size = item_count * block_size;

    /* Create the memory pool */
    allocator->memory_pool = k_malloc(pool_size);
    if (allocator->memory_pool == NULL) {
        LOG_ERR("Failed to allocate memory from the heap for the item buffer :(");
        return -ENOMEM;
    }
    
    memset(allocator->memory_pool, 0, pool_size);

    /* Create all memory blocks */
    allocator->free_list = NULL;
    for (int i=0; i<item_count; i++) {

        mb = (struct memory_block *)((char *)allocator->memory_pool + i * block_size + item_size);
        mb->data = (void *)((char *)allocator->memory_pool + i * block_size);    // to verify
        //LOG_DBG("New memory block @ %p with data @ %p\n", mb, mb->data);
        mb->next = allocator->free_list;
        allocator->free_list = mb;
    }

    allocator->capacity = item_count;
    allocator->item_size = item_size;
    allocator->count = 0;
    return 1;
}

static void clean_malloc_item(struct srcc_alloc *allocator)
{
    /* Verify all memory blocks are free before cleaning up...usefull?
    if (allocator->count != allocator->capacity) {
        LOG_ERR("Free all allocated items before cleaning\n");
        LOG_DBG("capacity = %d alloc_count = %d\n",
                allocator->capacity, allocator->count);

    } else {
    */

    k_free(allocator->memory_pool);
}

static void *malloc_item(struct srcc_alloc *allocator)
{
    if (allocator->free_list == NULL) {
        LOG_ERR("Memory pool is full :(\n");
        LOG_DBG("capacity = %d alloc_count = %d\n",
                allocator->capacity, allocator->count);
        return NULL;
    }

    struct memory_block *mb = allocator->free_list;
    allocator->free_list = mb->next;
    mb->next = NULL;

    allocator->count++;
    return mb->data;
}


static void free_item(struct srcc_alloc *allocator, void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    //LOG_DBG("Free: capacity = %d alloc_count = %d\n", allocator->capacity, allocator->count);

    struct memory_block *mb = (struct memory_block *)((char *)ptr + allocator->item_size);
    //LOG_DBG("ptr @ 0x%08x ?= data %p mb @ %p\n", (unsigned int) ptr, mb->data, mb);

    //memset(mb->data, 0, sizeof(allocator->item_size));

    mb->next = allocator->free_list;
    allocator->free_list = mb;
    allocator->count--;
}


/* API */


/* Scan allocator */
int srcc_init_scan_alloc(uint32_t count)
{
    return init_malloc_item(&scan_allocator, count, sizeof(struct srcc_scan_item));
}

void srcc_clean_scan_alloc(void)
{
    return clean_malloc_item(&scan_allocator);
}

void *srcc_malloc_scan_item(void)
{
    return malloc_item(&scan_allocator);
}

void srcc_free_scan_item(void *ptr)
{
    return free_item(&scan_allocator, ptr);
}


/* Conn allocator */
int srcc_init_conn_alloc(uint32_t count)
{
    return init_malloc_item(&conn_allocator, count, sizeof(struct srcc_conn_item));
}

void srcc_clean_conn_alloc(void)
{
    return clean_malloc_item(&conn_allocator);
}

void *srcc_malloc_conn_item(void)
{
    return malloc_item(&conn_allocator);
}

void srcc_free_conn_item(void *ptr)
{
    return free_item(&conn_allocator, ptr);
}


/* Advertisement allocator */
int srcc_init_adv_alloc(uint32_t count)
{
    return init_malloc_item(&adv_allocator, count, sizeof(struct srcc_adv_item));
}

void srcc_clean_adv_alloc(void)
{
    return clean_malloc_item(&adv_allocator);
}

void *srcc_malloc_adv_item(void)
{
    return malloc_item(&adv_allocator);
}

void srcc_free_adv_item(void *ptr)
{
    return free_item(&adv_allocator, ptr);
}
