/*
 *
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/sirocco.h>

/* An interface to control the memory allocation for FIFO metric items.
 */

struct memory_block {
    struct metric_item data;
    struct memory_block *next;
};

struct srcc_item_allocator {
    struct memory_block *free_list;
    //struct memory_block *in_use_list;
    void *memory_pool;
    uint32_t capacity;
    uint32_t count;
};


static struct srcc_item_allocator allocator = {
    NULL,
    NULL,
    0,
    0,
};


static void printk_free_list()
{
    struct memory_block *mb;
    uint32_t count;

    mb = allocator.free_list;
    count = 0;

    printk("Free list: ");
    while (mb != NULL) {
        count++;
        printk("%p --> ", mb);
        mb = mb->next;
    }
    printk("NULL\n");
}

int srcc_init_malloc_item(uint32_t nb)
{
    size_t size;
    struct memory_block *mb;

    size = nb*sizeof(struct memory_block);

    allocator.memory_pool = k_malloc(size);
    if (allocator.memory_pool == NULL) {
        printk("Failed to allocate memory from the heap for the item buffer :(\n");
        return -ENOMEM;
    }
    
    memset(allocator.memory_pool, 0, size);

    allocator.free_list = NULL;
    for (int i=0; i<nb; i++) {
        mb = (struct memory_block *)allocator.memory_pool + i;
        //printk("New memory block @ %p\n", mb);
        mb->next = allocator.free_list;
        allocator.free_list = mb;
    }

    printk_free_list();
    allocator.capacity = nb;
    allocator.count = 0;
    return 0;
}

void srcc_clean_malloc_item(void)
{
    /* Verify all memory blocks are free before cleaning up*/
    if (allocator.count != allocator.capacity) {
        printk("Free all allocated items before cleaning\n");
        printk("capacity = %d alloc_count = %d\n", allocator.capacity, allocator.count);

    } else {
        k_free(allocator.memory_pool);
    }
}

void *srcc_malloc_item(void)
{
    if (allocator.free_list == NULL) {
        printk("Memory pool is full :(\n");
        printk("capacity = %d alloc_count = %d\n", allocator.capacity, allocator.count);
        return NULL;
    }

    struct memory_block *mb = allocator.free_list;
    allocator.free_list = mb->next;
    mb->next = NULL;

    allocator.count++;
    return &(mb->data);
}


void srcc_free_item(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    /* To test...
    if ((ptr < allocator.memory_pool) || (ptr > ((struct memory_block *)allocator.memory_pool + allocator.capacity))) {
        return;
    }

    if (((unsigned int) ptr % sizeof(struct memory_block)) != 0) {
        return;
    }
    */

    //printk("Free: capacity = %d alloc_count = %d\n", allocator.capacity, allocator.count);

    struct memory_block *mb = (struct memory_block *)((char *)ptr - offsetof(struct memory_block, data));

    memset(mb, 0, sizeof(struct memory_block));

    mb->next = allocator.free_list;
    allocator.free_list = mb;
    allocator.count--;
}
