#include <stdint.h>
#include <stddef.h>

#include <memory.h>

static object_t *object_alloc(cache_t *);
static void slab_cache_grow(uint32_t, uint32_t);
static void cache_grow(cache_t *);
static cache_t *cache_create(uint32_t);
static void cache_init(cache_t *, uint32_t);

static cache_t *cache_chain = NULL;
static cache_t *cache_cache = NULL;
static cache_t *slab_cache = NULL;

/**
 * @brief Allocates an object from a cache.
 *
 * This function retrieves a free object from @p cache by searching through
 * partially filled and empty slabs. If no slabs with free objects are available
 * the cache is grown. When an object is allocated, the slab's in-use count is
 * incremented, and the slab may be moved between different lists based on its
 * occupancy.
 *
 * @param cache Pointer to the cache from which to allocate an object.
 * @return Pointer to the allocated object, or NULL if allocation fails.
 */
static object_t *object_alloc(cache_t *cache) {
    if (cache->slabs_empty == NULL && cache->slabs_partial == NULL) cache_grow(cache);
     
    if (cache->slabs_partial != NULL) {
        slab_t *target_slab = cache->slabs_partial;

        object_t *obj = target_slab->head;

        target_slab->head = obj->next;
        target_slab->inuse++;

        if (target_slab->inuse == cache->num) {
            cache->slabs_partial = NULL;
            cache->slabs_full = target_slab;
        }

        return obj;
    }

    if (cache->slabs_empty != NULL) {
        slab_t *target_slab = cache->slabs_empty;

        object_t *obj = target_slab->head;

        target_slab->head = obj->next;
        target_slab->inuse++;

        if (target_slab->inuse == cache->num) {
            cache->slabs_empty = NULL;
            cache->slabs_full = target_slab;
        } else {
            cache->slabs_empty = NULL;
            cache->slabs_partial = target_slab;
        }

        return obj;
    }
    return NULL; // This should never happen
}

/**
 * @brief Grows the slab cache by populating it with new slab objects.
 *
 * This function is used for growing the slab cache. Growing the slab cache
 * requires alllocating additional memory from the virtual memory manager. It
 * creates a linked list of slab objects within the allocated memory range. All
 * created slab objects are added to the empty slabs list of the slab cache.
 */
static void slab_cache_grow(uint32_t base, uint32_t length) {
    slab_t *slab = (slab_t *)(base);
    slab->head = NULL;
    slab->inuse = 0;
    
    uint32_t end = base + length;
    
    base += sizeof(slab_t);

    // Create and link slab objects
    while (base + sizeof(object_t) < end) {
        object_t *obj = (object_t *)base;

        obj->next = slab->head;
        slab->head = obj;

        base += sizeof(object_t);
    }
    
    slab_cache->slabs_empty = slab;
}

/**
 * @brief Grows a cache by adding a new slab with objects.
 *
 * This function expands a cache's capacity by allocating a new slab structure
 * from the slab cache and a 4 KiB page of virtual memory to hold the objects.
 * The new page is divided into objects of the cache's object size, and all
 * objects are linked together in a free list. The new slab is added to the
 * cache's empty slabs list. If the slab cache itself needs space, it is grown
 * first.
 *
 * @param cache Pointer to the cache to grow.
 */
static void cache_grow(cache_t *cache) {
   if (slab_cache->slabs_empty == NULL && slab_cache->slabs_partial == NULL) {
        uint32_t *addr = vmm_malloc(4096);

        if (addr == NULL) { /* TODO: handle out of virtual memory */ }

        slab_cache_grow((uint32_t)addr, 4096);
   }

   object_t *slab_obj = object_alloc(slab_cache);

   slab_t *new_slab = (slab_t *)slab_obj;
   new_slab->head = NULL;
   new_slab->inuse = 0;

   uint32_t *addr = vmm_malloc(4096);

   if (addr == NULL) { /* TODO: handle out of virtual memory */ }

   // Create and link 4 KiB of objects for the slab
   uint32_t end = (uint32_t)addr + 4096;

   while ((uint32_t)addr + cache->objsize < end) {
        object_t *obj = (object_t *)addr;

        obj->next = new_slab->head;
        new_slab->head = obj;

        addr = (uint32_t *)((uint32_t)addr + cache->objsize);
   }

   cache->slabs_empty = new_slab;
}

/**
 * @brief Creates a new cache for objects of a specific size.
 *
 * This function allocates a new cache structure from the cache of caches,
 * initializes it for the specified object size, and grows it to contain an
 * initial slab of objects. If the cache of caches has no free cache structures
 * available, it is grown first.
 *
 * @param objsize The size of objects that will be stored in this cache.
 * @return Pointer to the newly created cache.
 */
static cache_t *cache_create(uint32_t objsize) {
    if (cache_cache->slabs_empty == NULL && cache_cache->slabs_partial == NULL) cache_grow(cache_cache);

    object_t *cache_obj = object_alloc(cache_cache);

    cache_t *new_cache = (cache_t *)cache_obj;

    cache_init(new_cache, objsize);
    cache_grow(new_cache);

    return new_cache;
}

/**
 * @brief Initializes a cache structure with specified parameters.
 *
 * This function sets up a cache_t structure by configuring its object size,
 * calculating the number of objects that fit in a 4 KiB slab, and initializing
 * all slab list pointers to NULL. The cache is prepared for use but contains no
 * slabs until it is grown.
 *
 * @param cache Pointer to the cache structure to initialize.
 * @param objsize The size of objects that will be stored in this cache.
 */
static void cache_init(cache_t *cache, uint32_t objsize) {
    cache->objsize = objsize;
    cache->num = 4096 / objsize;
    cache->next = NULL;
    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_empty = NULL;
}

/**
 * @brief Initializes the kernel memory allocator (slab allocator).
 *
 * This function sets up the kernel heap manager. It initializes two special
 * caches, one for slab structures and one for cache structures, using a single
 * 4 KiB page from the virtual memory manager. It then creates a chain of
 * general-purpose caches for object sizes from 32 bytes to 2048 bytes.
 */
void kmem_init() {
    // Allocate one page for slab struct cache and the cache's slab objects
    uint32_t addr = (uint32_t)vmm_malloc(4096);

    // Initialize slab struct cache
    slab_cache = (cache_t *)addr;
    cache_init(slab_cache, sizeof(slab_t));
    addr = addr + sizeof(cache_t);
    
    // Initialize the cache struct cache
    cache_cache = (cache_t *)(addr);
    cache_init(cache_cache, sizeof(cache_t));
    addr = addr + sizeof(cache_t);

    // Add first slabs to the slab cache
    slab_cache_grow(addr, 4096 - (2 * sizeof(cache_t)));
    
    // Use slab cache to grow cache_cache
    cache_grow(cache_cache);

    // Create general purpose caches for sizes 2^5 to 2^11
    for (int i = 11; i >= 5; i--) {
        cache_t *cache = cache_create(1 << i);

        if (cache_chain == NULL) cache_chain = cache;

        cache->next = cache_chain;
        cache_chain = cache;
    }
}

/**
 * @brief Allocates kernel memory of the requested size.
 *
 * This function provides dynamic memory allocation for the kernel. For
 * allocations larger than 2048 bytes, memory is allocated directly from the
 * virtual memory manager. For smaller allocations, the function searches the
 * cache chain to find the smallest cache that can accommodate the requested
 * size, then allocates an object from that cache.
 *
 * @param length The size of memory to allocate (in bytes).
 * @return Pointer to the allocated memory, or NULL if allocation fails.
 */
void *kmalloc(uint32_t length) {
    if (length > 2048) return vmm_malloc(length);

    cache_t *curr = cache_chain;

    while (curr != NULL) {
        if (curr->objsize >= length) return object_alloc(curr);

        curr = curr->next;
    }

    return NULL;
}

/**
 * @brief Frees previously allocated kernel memory.
 *
 * This function returns memory to the kernel memory manager. For allocations
 * larger than 2048 bytes, memory is freed directly through the virtual memory
 * manager. For smaller allocations, the function finds the appropriate cache in
 * the cache chain and returns the object to its slab. When an object is freed,
 * the slab's in-use count is decremented, and the slab may be moved between
 * lists based on its new occupancy state.
 *
 * @param obj Pointer to the memory to free.
 * @param length The size of the memory allocation (in bytes).
 */
void kfree(void *obj, uint32_t length) {
    uint32_t addr = (uint32_t)obj;
    
    if (length > 2048) {
        vmm_free(addr, length);
        return;
    }

    cache_t *curr = cache_chain;

    while (curr->next != NULL) {  
        if (curr->objsize >= length) break;

        curr = curr->next;
    }

    // Return the object to its slab in the corresponding cache
    if (curr->slabs_full != NULL) {
        slab_t *target_slab = curr->slabs_full;

        object_t *head = target_slab->head;

        head->next = target_slab->head;
        target_slab->head = head;
        target_slab->inuse--;

        curr->slabs_full = NULL;
        curr->slabs_partial = target_slab;
    } else if (curr->slabs_partial != NULL) {
        slab_t *target_slab = curr->slabs_partial;

        object_t *head = target_slab->head;

        head->next = target_slab->head;
        target_slab->head = head;
        target_slab->inuse--;

        if (target_slab->inuse == 0) {
            curr->slabs_partial = NULL;
            curr->slabs_empty = target_slab;
        }
    }
}