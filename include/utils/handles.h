#ifndef HANDLES_H
#define HANDLES_H

#include "engine.h"
#include "flex_array.h"
#include <stdint.h>

/*
 * This is used for generating handles with generation tracking
 * 
 * HANDLE ID explanation:
 * A handle id stores both an index and a generation number
 *
 * The index:
 *      The index is the index in a flex_array that the handle points to
 *      The first 20 bits are the handle index | 16 bit example: 1000    000000000100
 *                                                               gen(8)  index(4)
 * The generation:
 *      This is a counter that is stored in an element in the flex_array
 *      Whenever an element is "removed", it is really just marked as inactive and kept there
 *      If this slot is then ever reused for a new resource,
 *        the generation number is incremented.
 * 
 * Slot reusage:
 *      In the case of the a slot being reused, a new handle is created for this element.
 *        If an old handle tries to get the element, the generations between the old and
 *         new element will be different meaning the element at the old generation's 
 *         index has been deleted and the slot was reused. Whatever resource the old handle
 *         was pointing to is now gone and replaced.
 *
 * This system assumes a lot of shit, and so it should only be used internally
 *
 * Specifically, this was created for handling resources, like Shaders and Textures
*/

/*
 * Assuming Name is the type the handle is used for
 * Assuming that type stores an `id` and `gen` fields that are uint32_t
 *
 * The first 20 bits in the uint32_t is used for storing the actual handle id
 * The last 12 bits are used to hold the generation number
*/
#define DECLARE_HANDLE(Name) \
    typedef struct { uint32_t id; } Name; \
    static inline uint32_t Name##_index(Name h) { return h.id & 0xFFFFF; } \
    static inline uint32_t Name##_gen(Name h)   { return h.id >> 20; } \
    static inline bool Name##_is_null(Name h)         { return h.id == 0; } \
    static inline Name Name##_null()            { return (Name){0}; } \
    static inline Name Name##_pack(uint32_t idx, uint32_t gen) { \
        return (Name){ .id = (gen << 20) | (idx & 0xFFFFF) }; \
    }


// This is needed for the free_list
DEFINE_FLEX_ARRAY(uint32_t, IndexArray);

#define DECLARE_RESOURCE_POOL(Type, ArrayType ) \
    typedef struct { \
        ArrayType* items; \
        IndexArray* free_list; \
    } Type##Pool; \
    extern Type##Pool _##Type##_pool;

#define DEFINE_RESOURCE_POOL(Type, HandleType, ArrayType) \
    Type##Pool _##Type##_pool = {0}; \
    \
    static HandleType Type##_alloc() { \
        uint32_t index; \
        if (_##Type##_pool.free_list && _##Type##_pool.free_list->count > 0) { \
            index = _##Type##_pool.free_list->data[--_##Type##_pool.free_list->count]; \
        } \
        else { \
            Type empty_item = {0}; \
            _##Type##_pool.items = ArrayType##_push(_##Type##_pool.items, empty_item); \
            index = _##Type##_pool.items->count - 1; \
        } \
        \
        Type* item = &_##Type##_pool.items->data[index]; \
        item->active = true; \
        \
        return HandleType##_pack(index, item->generation); \
    } \
    \
    static void Type##_free(HandleType h) { \
        uint32_t index = HandleType##_index(h); \
        Type* item = &_##Type##_pool.items->data[index]; \
        if (!item->active || item->generation != HandleType##_gen(h)) return; \
        \
        item->active = false; \
        item->generation++; \
        _##Type##_pool.free_list = IndexArray_push(_##Type##_pool.free_list, index); \
    } \
    \
    static void Type##_pool_shutdown(void (*destructor)(Type*)) { \
        if (!_##Type##_pool.items) return; \
        \
        /* Call destructor callback if provided */ \
        if (destructor) { \
            for (uint32_t i = 0; i < _##Type##_pool.items->count; i++) { \
                Type* item = &_##Type##_pool.items->data[i]; \
                if (item->active) { \
                    destructor(item); \
                } \
            } \
        } \
        \
        /* Free management arrays */ \
        ArrayType##_free(_##Type##_pool.items); \
        IndexArray_free(_##Type##_pool.free_list); \
        \
        _##Type##_pool.items = NULL; \
        _##Type##_pool.free_list = NULL; \
    }

/* -------- Resource getter function -------*/

#ifdef DEBUG_BUILD

// Debug getter has all the safety check for debugging in development
#define DEFINE_RESOURCE_GETTER(ReturnType, HandleType, PoolVar, LogName) \
    static inline ReturnType* ReturnType##_get(HandleType h) { \
        uint32_t index = HandleType##_index(h); \
        uint32_t gen   = HandleType##_gen(h); \
        \
        /* System initialized and bounds check */ \
        if(UNLIKELY(!PoolVar.items || index >= PoolVar.items->count)) { \
            LOG_ERROR("[%s]: Handle index %u out of bounds or system uninit.", LogName, index); \
            \
            /* Return fallback (first item) if present */ \
            if (PoolVar.items && PoolVar.items->count > 0) return &PoolVar.items->data[0]; \
            return NULL; \
        } \
        \
        ReturnType* item = &PoolVar.items->data[index]; \
        \
        /* Index 0 is the fallback. Can't be deleted */ \
        if (index == 0) return item; \
        \
        /* Check if handle is stale (generation mismatch or deleted handle) */ \
        if (UNLIKELY(!item->active || item->generation != gen)) { \
            LOG_WARN("[%s]: Accessed deleted resource handle (Idx: %u, Gen %u). Returning fallback.\n", \
                    LogName, index, gen); \
            return &PoolVar.items->data[0]; \
        } \
        return item; \
    }

#else

// Release build has zero safety checks for maximum speed
#define DEFINE_RESOURCE_GETTER(ReturnType, HandleType, PoolVar, LogName) \
    static inline ReturnType* ReturnType##_get(HandleType h) { \
        uint32_t index = HandleType##_index(h); \
        return &PoolVar.items->data[index]; \
    }

#endif

#endif // !HANDLES_H
